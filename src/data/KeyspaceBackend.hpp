//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "data/CassandraBackend.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/LedgerHeaderCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/CassandraBackendFamily.hpp"
#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/KeyspaceSchema.hpp"
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <tuple>
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
class BasicKeyspaceBackend : public DefaultCassandraFamily {
    KeyspaceSchema<SettingsProviderType>* keyspaceSchema_;

public:
    BasicKeyspaceBackend(SettingsProviderType settingsProvider, data::LedgerCacheInterface& cache, bool readOnly)
        : DefaultCassandraFamily(
              settingsProvider,
              std::make_unique<KeyspaceSchema<SettingsProviderType>>(settingsProvider),
              cache,
              readOnly
          )
    {
        // cast the pointer to KeyspaceSchema type as there is a few statements unique to KeyspaceBackend
        keyspaceSchema_ = static_cast<KeyspaceSchema<SettingsProviderType>*>(this->schema_.get());
    }

    bool
    doFinishWrites() override
    {
        waitForWritesToFinish();

        // !range_.has_value() means the table 'ledger_range' is not populated;
        // This would be the first write to the table.
        // In this case, insert both min_sequence/max_sequence range into the table.
        if (!range_.has_value()) {
            executor_.writeSync(keyspaceSchema_->insertLedgerRange(), false, ledgerSequence_);
            executor_.writeSync(keyspaceSchema_->insertLedgerRange(), true, ledgerSequence_);
        }

        if (not executeSyncUpdate(
                keyspaceSchema_->updateLedgerRange().bind(ledgerSequence_, true, ledgerSequence_ - 1)
            )) {
            log_.warn() << "Update failed for ledger " << ledgerSequence_;
            return false;
        }

        log_.info() << "Committed ledger " << ledgerSequence_;
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
        std::vector<ripple::uint256> nftIDs;
        if (taxon.has_value()) {
            // Keyspace and ScyllaDB can use the same logic for taxon-filtered queries
            nftIDs = fetchNFTIDsByTaxon(issuer, *taxon, limit, cursorIn, yield);
        } else {
            // --- Amazon Keyspaces Workflow for non-taxon queries ---
            auto const startTaxon = cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)) : 0;
            auto const startTokenID = cursorIn.value_or(ripple::uint256(0));

            Statement firstQuery = keyspaceSchema_->selectNFTIDsByIssuerTaxon().bind(issuer);
            firstQuery.bindAt(1, startTaxon);
            firstQuery.bindAt(2, startTokenID);
            firstQuery.bindAt(3, Limit{limit});

            auto const firstRes = executor_.read(yield, firstQuery);
            if (firstRes) {
                for (auto const [nftID] : extract<ripple::uint256>(firstRes.value()))
                    nftIDs.push_back(nftID);
            }

            if (nftIDs.size() < limit) {
                auto const remainingLimit = limit - nftIDs.size();
                Statement secondQuery = keyspaceSchema_->selectNFTsAfterTaxonKeyspaces().bind(issuer);
                secondQuery.bindAt(1, startTaxon);
                secondQuery.bindAt(2, Limit{remainingLimit});

                auto const secondRes = executor_.read(yield, secondQuery);
                if (secondRes) {
                    for (auto const [nftID] : extract<ripple::uint256>(secondRes.value()))
                        nftIDs.push_back(nftID);
                }
            }
        }
        return populateNFTsAndCreateCursor(nftIDs, ledgerSequence, limit, yield);
    }

    /**
     * @brief Loading cache with account is currently unsupported by aws keyspace backend.
     * The reason is because this function calls statements (selectAccountFromToken, selectaccountfrombeginning)
     * that uses "PER PARTITION LIMIT 1". As keyspace currently doesn't support "PER PARTITION LIMIT" and there is
     * no good way to filter out the result, we are disabling this feature for now. This should be okay for now as
     * we load cache by diff or cursor from diff, rarely by accounts.
     */
    std::vector<ripple::uint256>
    fetchAccountRoots(
        [[maybe_unused]] std::uint32_t number,
        [[maybe_unused]] std::uint32_t pageSize,
        [[maybe_unused]] std::uint32_t seq,
        [[maybe_unused]] boost::asio::yield_context yield
    ) const override
    {
        LOG(log_.error()) << "Fetching account roots is not supported by the Keyspaces backend.";
        throw std::runtime_error("Fetching all account roots is not supported by the Keyspaces backend.");
    }

private:
    bool
    executeSyncUpdate(Statement statement)
    {
        auto const res = executor_.writeSync(statement);
        auto maybeSuccess = res->template get<bool>();
        if (not maybeSuccess) {
            LOG(log_.error()) << "executeSyncUpdate - error getting result - no row";
            return false;
        }

        if (not maybeSuccess.value()) {
            LOG(log_.warn()) << "Update failed. Checking if DB state is what we expect";

            // error may indicate that another writer wrote something.
            // in this case let's just compare the current state of things
            // against what we were trying to write in the first place and
            // use that as the source of truth for the result.
            auto rng = hardFetchLedgerRangeNoThrow();
            return rng && rng->maxSequence == ledgerSequence_;
        }

        return true;
    }

    std::vector<ripple::uint256>
    fetchNFTIDsByTaxon(
        ripple::AccountID const& issuer,
        std::uint32_t const taxon,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context yield
    ) const
    {
        std::vector<ripple::uint256> nftIDs;
        Statement statement = keyspaceSchema_->selectNFTIDsByIssuerTaxon().bind(issuer);
        statement.bindAt(1, taxon);
        statement.bindAt(2, cursorIn.value_or(ripple::uint256(0)));
        statement.bindAt(3, Limit{limit});

        auto const res = executor_.read(yield, statement);
        if (res && res.value().hasRows()) {
            for (auto const [nftID] : extract<ripple::uint256>(res.value()))
                nftIDs.push_back(nftID);
        }
        return nftIDs;
    }

    std::vector<ripple::uint256>
    fetchNFTIDsWithoutTaxon(
        ripple::AccountID const& issuer,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context yield
    ) const
    {
        std::vector<ripple::uint256> nftIDs;

        auto const startTaxon = cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)) : 0;
        auto const startTokenID = cursorIn.value_or(ripple::uint256(0));

        Statement firstQuery = keyspaceSchema_->selectNFTIDsByIssuerTaxon().bind(issuer);
        firstQuery.bindAt(1, startTaxon);
        firstQuery.bindAt(2, startTokenID);
        firstQuery.bindAt(3, Limit{limit});

        auto const firstRes = executor_.read(yield, firstQuery);
        if (firstRes) {
            for (auto const [nftID] : extract<ripple::uint256>(firstRes.value()))
                nftIDs.push_back(nftID);
        }

        if (nftIDs.size() < limit) {
            auto const remainingLimit = limit - nftIDs.size();
            Statement secondQuery = keyspaceSchema_->selectNFTsAfterTaxonKeyspaces().bind(issuer);
            secondQuery.bindAt(1, startTaxon);
            secondQuery.bindAt(2, Limit{remainingLimit});

            auto const secondRes = executor_.read(yield, secondQuery);
            if (secondRes) {
                for (auto const [nftID] : extract<ripple::uint256>(secondRes.value()))
                    nftIDs.push_back(nftID);
            }
        }
        return nftIDs;
    }

    /**
     * @brief Takes a list of NFT IDs, fetches their full data, and assembles the final result with a cursor.
     */
    NFTsAndCursor
    populateNFTsAndCreateCursor(
        std::vector<ripple::uint256> const& nftIDs,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        boost::asio::yield_context yield
    ) const
    {
        if (nftIDs.empty()) {
            LOG(log_.debug()) << "No rows returned";
            return {};
        }

        NFTsAndCursor ret;
        if (nftIDs.size() == limit)
            ret.cursor = nftIDs.back();

        // Prepare and execute queries to fetch NFT info and URIs in parallel.
        std::vector<Statement> selectNFTStatements;
        selectNFTStatements.reserve(nftIDs.size());
        std::transform(
            std::cbegin(nftIDs), std::cend(nftIDs), std::back_inserter(selectNFTStatements), [&](auto const& nftID) {
                return keyspaceSchema_->selectNFT().bind(nftID, ledgerSequence);
            }
        );

        std::vector<Statement> selectNFTURIStatements;
        selectNFTURIStatements.reserve(nftIDs.size());
        std::transform(
            std::cbegin(nftIDs), std::cend(nftIDs), std::back_inserter(selectNFTURIStatements), [&](auto const& nftID) {
                return keyspaceSchema_->selectNFTURI().bind(nftID, ledgerSequence);
            }
        );

        auto const nftInfos = executor_.readEach(yield, selectNFTStatements);
        auto const nftUris = executor_.readEach(yield, selectNFTURIStatements);

        // Combine the results into final NFT objects.
        for (auto i = 0u; i < nftIDs.size(); ++i) {
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
};

using KeyspaceBackend = BasicKeyspaceBackend<SettingsProvider, impl::DefaultExecutionStrategy<>>;

}  // namespace data::cassandra
