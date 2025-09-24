//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/LedgerHeaderCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/CassandraBackendFamily.hpp"
#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ExecutionStrategy.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class CacheBackendCassandraTest;

namespace data::cassandra {

/**
 * @brief Implements @ref BackendInterface for Cassandra/ScyllaDB.
 *
 * Note: This is a safer and more correct rewrite of the original implementation of the backend.
 *
 * @tparam SettingsProviderType The settings provider type to use
 * @tparam ExecutionStrategyType The execution strategy type to use
 * @tparam FetchLedgerCacheType The ledger header cache type to use
 */
template <
    SomeSettingsProvider SettingsProviderType,
    SomeExecutionStrategy ExecutionStrategyType,
    typename FetchLedgerCacheType = FetchLedgerCache>
class BasicCassandraBackend
    : public CassandraBackendFamily<SettingsProviderType, ExecutionStrategyType, FetchLedgerCacheType> {
    using DefaultCassandraFamily =
        CassandraBackendFamily<SettingsProviderType, ExecutionStrategyType, FetchLedgerCacheType>;

    Schema<SettingsProviderType>* schemaPtr_{};

public:
    BasicCassandraBackend(SettingsProviderType settingsProvider, data::LedgerCacheInterface& cache, bool readOnly)
        : DefaultCassandraFamily(
              settingsProvider,
              std::make_unique<Schema<SettingsProviderType>>(settingsProvider),
              cache,
              readOnly
          )
    {
        // cast the pointer to Schema type as there is a few statements unique to CassandraBackend
        schemaPtr_ = static_cast<Schema<SettingsProviderType>*>(this->schema_.get());
    }
    /*
     * @brief Move constructor is deleted because handle_ is shared by reference with executor
     */
    BasicCassandraBackend(BasicCassandraBackend&&) = delete;

    bool
    doFinishWrites() override
    {
        this->waitForWritesToFinish();

        if (!this->range_) {
            this->executor_.writeSync(
                schemaPtr_->updateLedgerRange(), this->ledgerSequence_, false, this->ledgerSequence_
            );
        }

        if (not executeSyncUpdate(
                schemaPtr_->updateLedgerRange().bind(this->ledgerSequence_, true, this->ledgerSequence_ - 1)
            )) {
            LOG(this->log_.warn()) << "Update failed for ledger " << this->ledgerSequence_;
            return false;
        }

        LOG(this->log_.info()) << "Committed ledger " << this->ledgerSequence_;
        return true;
    }

    NFTsAndCursor
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
                auto r = schemaPtr_->selectNFTIDsByIssuerTaxon().bind(issuer);
                r.bindAt(1, *taxon);
                r.bindAt(2, cursorIn.value_or(ripple::uint256(0)));
                r.bindAt(3, Limit{limit});
                return r;
            }

            auto r = schemaPtr_->selectNFTIDsByIssuer().bind(issuer);
            r.bindAt(
                1,
                std::make_tuple(
                    cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)) : 0,
                    cursorIn.value_or(ripple::uint256(0))
                )
            );
            r.bindAt(2, Limit{limit});
            return r;
        }();

        // Query for all the NFTs issued by the account, potentially filtered by the taxon
        auto const res = this->executor_.read(yield, idQueryStatement);

        auto const& idQueryResults = res.value();
        if (not idQueryResults.hasRows()) {
            LOG(this->log_.debug()) << "No rows returned";
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
            std::cbegin(nftIDs), std::cend(nftIDs), std::back_inserter(selectNFTStatements), [&](auto const& nftID) {
                return schemaPtr_->selectNFT().bind(nftID, ledgerSequence);
            }
        );

        auto const nftInfos = this->executor_.readEach(yield, selectNFTStatements);

        std::vector<Statement> selectNFTURIStatements;
        selectNFTURIStatements.reserve(nftIDs.size());

        std::transform(
            std::cbegin(nftIDs), std::cend(nftIDs), std::back_inserter(selectNFTURIStatements), [&](auto const& nftID) {
                return schemaPtr_->selectNFTURI().bind(nftID, ledgerSequence);
            }
        );

        auto const nftUris = this->executor_.readEach(yield, selectNFTURIStatements);

        for (auto i = 0u; i < nftIDs.size(); i++) {
            if (auto const maybeRow = nftInfos[i].template get<uint32_t, ripple::AccountID, bool>(); maybeRow) {
                auto [seq, owner, isBurned] = *maybeRow;
                NFT nft(nftIDs[i], seq, owner, isBurned);
                if (auto const maybeUri = nftUris[i].template get<ripple::Blob>(); maybeUri)
                    nft.uri = *maybeUri;
                ret.nfts.push_back(nft);
            }
        }
        return ret;
    }

    std::vector<ripple::uint256>
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
            Statement const statement = lastItem ? schemaPtr_->selectAccountFromToken().bind(*lastItem, Limit{pageSize})
                                                 : schemaPtr_->selectAccountFromBeginning().bind(Limit{pageSize});

            auto const res = this->executor_.read(yield, statement);
            if (res) {
                auto const& results = res.value();
                if (not results.hasRows()) {
                    LOG(this->log_.debug()) << "No rows returned";
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
                LOG(this->log_.error()) << "Could not fetch account from account_tx: " << res.error();
                break;
            }
        }

        return liveAccounts;
    }

private:
    bool
    executeSyncUpdate(Statement statement)
    {
        auto const res = this->executor_.writeSync(statement);
        auto maybeSuccess = res->template get<bool>();
        if (not maybeSuccess) {
            LOG(this->log_.error()) << "executeSyncUpdate - error getting result - no row";
            return false;
        }

        if (not maybeSuccess.value()) {
            LOG(this->log_.warn()) << "Update failed. Checking if DB state is what we expect";

            // error may indicate that another writer wrote something.
            // in this case let's just compare the current state of things
            // against what we were trying to write in the first place and
            // use that as the source of truth for the result.
            auto rng = this->hardFetchLedgerRangeNoThrow();
            return rng && rng->maxSequence == this->ledgerSequence_;
        }

        return true;
    }
};

using CassandraBackend = BasicCassandraBackend<SettingsProvider, impl::DefaultExecutionStrategy<>>;

}  // namespace data::cassandra
