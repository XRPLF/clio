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

#include "data/LedgerHeaderCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/CassandraBackendFamily.hpp"
#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/KeyspaceSchema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ExecutionStrategy.hpp"
#include "util/Assert.hpp"
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

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace data::cassandra {

/**
 * @brief Implements @ref CassandraBackendFamily for Keyspace
 *
 * @tparam SettingsProviderType The settings provider type to use
 * @tparam ExecutionStrategyType The execution strategy type to use
 * @tparam FetchLedgerCacheType The ledger header cache type to use
 */
template <
    SomeSettingsProvider SettingsProviderType,
    SomeExecutionStrategy ExecutionStrategyType,
    typename FetchLedgerCacheType = FetchLedgerCache>
class BasicKeyspaceBackend : public CassandraBackendFamily<
                                 SettingsProviderType,
                                 ExecutionStrategyType,
                                 KeyspaceSchema<SettingsProviderType>,
                                 FetchLedgerCacheType> {
    using DefaultCassandraFamily = CassandraBackendFamily<
        SettingsProviderType,
        ExecutionStrategyType,
        KeyspaceSchema<SettingsProviderType>,
        FetchLedgerCacheType>;

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

    /**
     * @brief Move constructor is deleted because handle_ is shared by reference with executor
     */
    BasicKeyspaceBackend(BasicKeyspaceBackend&&) = delete;

    bool
    doFinishWrites() override
    {
        this->waitForWritesToFinish();

        // !range_.has_value() means the table 'ledger_range' is not populated;
        // This would be the first write to the table.
        // In this case, insert both min_sequence/max_sequence range into the table.
        if (not(range_.has_value())) {
            executor_.writeSync(schema_->insertLedgerRange, false, ledgerSequence_);
            executor_.writeSync(schema_->insertLedgerRange, true, ledgerSequence_);
        }

        if (not this->executeSyncUpdate(schema_->updateLedgerRange.bind(ledgerSequence_, true, ledgerSequence_ - 1))) {
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
            // Keyspace and ScyllaDB uses the same logic for taxon-filtered queries
            nftIDs = fetchNFTIDsByTaxon(issuer, *taxon, limit, cursorIn, yield);
        } else {
            // --- Amazon Keyspaces Workflow for non-taxon queries ---
            auto const startTaxon = cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)) : 0;
            auto const startTokenID = cursorIn.value_or(ripple::uint256(0));

            Statement firstQuery = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
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
                Statement secondQuery = schema_->selectNFTsAfterTaxonKeyspaces.bind(issuer);
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
     * @brief (Unsupported in Keyspaces) Fetches account root object indexes by page.
     * * @note Loading the cache by enumerating all accounts is currently unsupported by the AWS Keyspaces backend.
     * This function's logic relies on "PER PARTITION LIMIT 1", which Keyspaces does not support, and there is
     * no efficient alternative. This is acceptable as the cache is primarily loaded via diffs. Calling this
     * function will throw an exception.
     *
     * @param number The total number of accounts to fetch.
     * @param pageSize The maximum number of accounts per page.
     * @param seq The accounts need to exist at this ledger sequence.
     * @param yield The coroutine context.
     * @return A vector of ripple::uint256 representing the account root hashes.
     */
    std::vector<ripple::uint256>
    fetchAccountRoots(
        [[maybe_unused]] std::uint32_t number,
        [[maybe_unused]] std::uint32_t pageSize,
        [[maybe_unused]] std::uint32_t seq,
        [[maybe_unused]] boost::asio::yield_context yield
    ) const override
    {
        ASSERT(false, "Fetching account roots is not supported by the Keyspaces backend.");
        std::unreachable();
    }

private:
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
        Statement statement = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
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

        Statement firstQuery = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
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
            Statement secondQuery = schema_->selectNFTsAfterTaxonKeyspaces.bind(issuer);
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
                return schema_->selectNFT.bind(nftID, ledgerSequence);
            }
        );

        std::vector<Statement> selectNFTURIStatements;
        selectNFTURIStatements.reserve(nftIDs.size());
        std::transform(
            std::cbegin(nftIDs), std::cend(nftIDs), std::back_inserter(selectNFTURIStatements), [&](auto const& nftID) {
                return schema_->selectNFTURI.bind(nftID, ledgerSequence);
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
