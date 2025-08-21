//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
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
#include "data/clickhouse/Concepts.hpp"
#include "data/clickhouse/Handle.hpp"
#include "data/clickhouse/Schema.hpp"
#include "data/clickhouse/SettingsProvider.hpp"
#include "data/clickhouse/Types.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
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
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace data::clickhouse {

/**
 * @brief Implements @ref BackendInterface for ClickHouse.
 *
 * This backend provides a ClickHouse implementation of the Clio backend interface.
 * ClickHouse is a column-oriented database that provides excellent performance for
 * analytical queries and is well-suited for storing ledger data.
 *
 * @tparam SettingsProviderType The settings provider type to use
 * @tparam FetchLedgerCacheType The ledger header cache type to use
 */
template <typename SettingsProviderType, typename FetchLedgerCacheType = FetchLedgerCache>
class BasicClickHouseBackend : public BackendInterface {
    util::Logger log_{"ClickHouseBackend"};

    SettingsProviderType settingsProvider_;
    Schema<SettingsProviderType> schema_;
    std::atomic_uint32_t ledgerSequence_ = 0u;

protected:
    Handle handle_;

    // have to be mutable because BackendInterface constness :(
    mutable FetchLedgerCacheType ledgerCache_{};

public:
    /**
     * @brief Create a new ClickHouse backend instance.
     *
     * @param settingsProvider The settings provider to use
     * @param cache The ledger cache to use
     * @param readOnly Whether the database should be in readonly mode
     */
    BasicClickHouseBackend(SettingsProviderType settingsProvider, data::LedgerCacheInterface& cache, bool readOnly)
        : BackendInterface(cache)
        , settingsProvider_{std::move(settingsProvider)}
        , schema_{settingsProvider_}
        , handle_{settingsProvider_.getSettings()}
    {
        if (auto const res = handle_.connect(); not res)
            throw std::runtime_error("Could not connect to ClickHouse database: " + res.error().message());

        if (not readOnly) {
            if (auto const res = handle_.execute(schema_.createDatabase); not res) {
                throw std::runtime_error("Could not create database: " + res.error().message());
            }

            if (auto const res = handle_.executeEach(schema_.createSchema); not res)
                throw std::runtime_error("Could not create schema: " + res.error().message());
        }

        try {
            schema_.prepareStatements(handle_);
        } catch (std::runtime_error const& ex) {
            auto const error = fmt::format(
                "Failed to prepare the statements: {}; readOnly: {}. ReadOnly should be turned off or another Clio "
                "node with write access to DB should be started first.",
                ex.what(),
                readOnly
            );
            LOG(log_.error()) << error;
            throw std::runtime_error(error);
        }
        LOG(log_.info()) << "Created ClickHouseBackend";
    }

    // Implement required pure virtual methods from BackendInterface
    std::optional<Blob>
    doFetchLedgerObject(ripple::uint256 const& key, std::uint32_t sequence, boost::asio::yield_context yield) const override;

    std::optional<std::uint32_t>
    doFetchLedgerObjectSeq(
        ripple::uint256 const& key,
        std::uint32_t sequence,
        boost::asio::yield_context yield
    ) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t sequence,
        boost::asio::yield_context yield
    ) const override;

    std::optional<ripple::uint256>
    doFetchSuccessorKey(ripple::uint256 key, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const override;

    std::optional<std::string>
    fetchMigratorStatus(std::string const& migratorName, boost::asio::yield_context yield) const override;

    ClioNodesDataFetchResult
    fetchClioNodesData(boost::asio::yield_context yield) const override;

    // Implement all required virtual methods from BackendInterface
    std::optional<ripple::LedgerHeader>
    fetchLedgerBySequence(std::uint32_t sequence, boost::asio::yield_context yield) const override;

    std::optional<ripple::LedgerHeader>
    fetchLedgerByHash(ripple::uint256 const& hash, boost::asio::yield_context yield) const override;

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context yield) const override;

    std::vector<ripple::uint256>
    fetchAccountRoots(
        std::uint32_t number,
        std::uint32_t pageSize,
        std::uint32_t seq,
        boost::asio::yield_context yield
    ) const override;

    std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash, boost::asio::yield_context yield) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes, boost::asio::yield_context yield) const override;

    TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context yield
    ) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const override;

    std::optional<NFT>
    fetchNFT(ripple::uint256 const& tokenID, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const override;

    TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context yield
    ) const override;

    NFTsAndCursor
    fetchNFTsByIssuer(
        ripple::AccountID const& issuer,
        std::optional<std::uint32_t> const& taxon,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context yield
    ) const override;

    MPTHoldersAndCursor
    fetchMPTHolders(
        ripple::uint192 const& mptID,
        std::uint32_t const limit,
        std::optional<ripple::AccountID> const& cursorIn,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context yield
    ) const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context yield) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const override;

    void
    writeLedger(ripple::LedgerHeader const& ledgerHeader, std::string&& blob) override;

    void
    writeTransaction(
        std::string&& hash,
        std::uint32_t seq,
        std::uint32_t date,
        std::string&& transaction,
        std::string&& metadata
    ) override;

    void
    writeNFTs(std::vector<NFTsData> const& data) override;

    void
    writeAccountTransactions(std::vector<AccountTransactionsData> data) override;

    void
    writeAccountTransaction(AccountTransactionsData record) override;

    void
    writeNFTTransactions(std::vector<NFTTransactionsData> const& data) override;

    void
    writeMPTHolders(std::vector<MPTHolderData> const& data) override;

    void
    writeSuccessor(std::string&& key, std::uint32_t seq, std::string&& successor) override;

    void
    writeNodeMessage(boost::uuids::uuid const& uuid, std::string message) override;

    void
    startWrites() const override;

    void
    waitForWritesToFinish() override;

    void
    writeMigratorStatus(std::string const& migratorName, std::string const& status) override;

    bool
    isTooBusy() const override;

    boost::json::object
    stats() const override;

private:
    bool
    executeSyncUpdate(std::string statement);

    void
    doWriteLedgerObject(std::string&& key, std::uint32_t seq, std::string&& blob) override;

    bool
    doFinishWrites() override;
};

using ClickHouseBackend = BasicClickHouseBackend<SettingsProvider>;

}  // namespace data::clickhouse
