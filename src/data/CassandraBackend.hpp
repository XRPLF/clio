#pragma once

#include "data/LedgerHeaderCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/CassandraBackendFamily.hpp"
#include "data/cassandra/CassandraSchema.hpp"
#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ExecutionStrategy.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <cassandra.h>
#include <fmt/format.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/nft.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace data::cassandra {

/**
 * @brief Implements @ref CassandraBackendFamily for Cassandra/ScyllaDB.
 *
 * @tparam SettingsProviderType The settings provider type to use
 * @tparam ExecutionStrategyType The execution strategy type to use
 * @tparam FetchLedgerCacheType The ledger header cache type to use
 */
template <
    SomeSettingsProvider SettingsProviderType,
    SomeExecutionStrategy ExecutionStrategyType,
    typename FetchLedgerCacheType = FetchLedgerCache>
class BasicCassandraBackend : public CassandraBackendFamily<
                                  SettingsProviderType,
                                  ExecutionStrategyType,
                                  CassandraSchema<SettingsProviderType>,
                                  FetchLedgerCacheType> {
    using DefaultCassandraFamily = CassandraBackendFamily<
        SettingsProviderType,
        ExecutionStrategyType,
        CassandraSchema<SettingsProviderType>,
        FetchLedgerCacheType>;

    // protected because CassandraMigrationBackend inherits from this class
protected:
    using DefaultCassandraFamily::executor_;
    using DefaultCassandraFamily::ledgerSequence_;
    using DefaultCassandraFamily::log_;
    using DefaultCassandraFamily::range_;
    using DefaultCassandraFamily::schema_;

public:
    /**
     * @brief Inherit the constructors of the base class.
     */
    using DefaultCassandraFamily::DefaultCassandraFamily;

    /*
     * @brief Move constructor is deleted because handle_ is shared by reference with executor
     */
    BasicCassandraBackend(BasicCassandraBackend&&) = delete;

    bool
    doFinishWrites() override
    {
        this->waitForWritesToFinish();

        if (!range_) {
            executor_.writeSync(
                schema_->updateLedgerRange, ledgerSequence_, false, ledgerSequence_
            );
        }

        if (not this->executeSyncUpdate(
                schema_->updateLedgerRange.bind(ledgerSequence_, true, ledgerSequence_ - 1)
            )) {
            LOG(log_.warn()) << "Update failed for ledger " << ledgerSequence_;
            return false;
        }

        LOG(log_.info()) << "Committed ledger " << ledgerSequence_;
        return true;
    }

    [[nodiscard]] NFTsAndCursor
    fetchNFTsByIssuer(
        ripple::AccountID const& issuer,
        std::optional<std::uint32_t> const& taxon,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context yield
    ) const override
    {
        NFTsAndCursor ret;

        Statement const idQueryStatement = [&taxon, &issuer, &cursorIn, &limit, this]() {
            if (taxon.has_value()) {
                auto r = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
                r.bindAt(1, *taxon);
                r.bindAt(2, cursorIn.value_or(ripple::uint256(0)));
                r.bindAt(3, Limit{limit});
                return r;
            }

            auto r = schema_->selectNFTIDsByIssuer.bind(issuer);
            r.bindAt(
                1,
                std::make_tuple(
                    cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn))
                                         : 0,
                    cursorIn.value_or(ripple::uint256(0))
                )
            );
            r.bindAt(2, Limit{limit});
            return r;
        }();

        // Query for all the NFTs issued by the account, potentially filtered by the taxon
        auto const res = executor_.read(yield, idQueryStatement);

        auto const& idQueryResults = res.value();
        if (not idQueryResults.hasRows()) {
            LOG(log_.debug()) << "No rows returned";
            return {};
        }

        std::vector<ripple::uint256> nftIDs;
        for (auto const [nftID] : extract<ripple::uint256>(idQueryResults))
            nftIDs.push_back(nftID);

        if (nftIDs.empty())
            return ret;

        if (nftIDs.size() == limit)
            ret.cursor = nftIDs.back();

        std::vector<Statement> selectNFTStatements;
        selectNFTStatements.reserve(nftIDs.size());

        std::transform(
            std::cbegin(nftIDs),
            std::cend(nftIDs),
            std::back_inserter(selectNFTStatements),
            [&](auto const& nftID) { return schema_->selectNFT.bind(nftID, ledgerSequence); }
        );

        auto const nftInfos = executor_.readEach(yield, selectNFTStatements);

        std::vector<Statement> selectNFTURIStatements;
        selectNFTURIStatements.reserve(nftIDs.size());

        std::transform(
            std::cbegin(nftIDs),
            std::cend(nftIDs),
            std::back_inserter(selectNFTURIStatements),
            [&](auto const& nftID) { return schema_->selectNFTURI.bind(nftID, ledgerSequence); }
        );

        auto const nftUris = executor_.readEach(yield, selectNFTURIStatements);

        for (auto i = 0u; i < nftIDs.size(); i++) {
            if (auto const maybeRow = nftInfos[i].template get<uint32_t, ripple::AccountID, bool>();
                maybeRow.has_value()) {
                auto [seq, owner, isBurned] = *maybeRow;
                NFT nft(nftIDs[i], seq, owner, isBurned);
                if (auto const maybeUri = nftUris[i].template get<ripple::Blob>();
                    maybeUri.has_value())
                    nft.uri = *maybeUri;
                ret.nfts.push_back(nft);
            }
        }
        return ret;
    }

    [[nodiscard]] std::vector<ripple::uint256>
    fetchAccountRoots(
        std::uint32_t number,
        std::uint32_t pageSize,
        std::uint32_t seq,
        boost::asio::yield_context yield
    ) const override
    {
        std::vector<ripple::uint256> liveAccounts;
        std::optional<ripple::AccountID> lastItem;

        while (liveAccounts.size() < number) {
            Statement const statement = lastItem
                ? schema_->selectAccountFromToken.bind(*lastItem, Limit{pageSize})
                : schema_->selectAccountFromBeginning.bind(Limit{pageSize});

            auto const res = executor_.read(yield, statement);
            if (res) {
                auto const& results = res.value();
                if (not results.hasRows()) {
                    LOG(log_.debug()) << "No rows returned";
                    break;
                }
                // The results should not contain duplicates, we just filter out deleted accounts
                std::vector<ripple::uint256> fullAccounts;
                for (auto [account] : extract<ripple::AccountID>(results)) {
                    fullAccounts.push_back(ripple::keylet::account(account).key);
                    lastItem = account;
                }
                auto const objs = this->doFetchLedgerObjects(fullAccounts, seq, yield);

                for (auto i = 0u; i < fullAccounts.size(); i++) {
                    if (not objs[i].empty()) {
                        if (liveAccounts.size() < number) {
                            liveAccounts.push_back(fullAccounts[i]);
                        } else {
                            break;
                        }
                    }
                }
            } else {
                LOG(log_.error()) << "Could not fetch account from account_tx: " << res.error();
                break;
            }
        }

        return liveAccounts;
    }
};

using CassandraBackend = BasicCassandraBackend<SettingsProvider, impl::DefaultExecutionStrategy<>>;

}  // namespace data::cassandra
