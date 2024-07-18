//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "data/DBHelpers.hpp"
#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "etl/CorruptionDetector.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/utility/result_of.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace data {

/**
 * @brief Represents a database timeout error.
 */
class DatabaseTimeout : public std::exception {
public:
    /**
     * @return The error message as a C string
     */
    char const*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};

static constexpr std::size_t DEFAULT_WAIT_BETWEEN_RETRY = 500;
/**
 * @brief A helper function that catches DatabaseTimout exceptions and retries indefinitely.
 *
 * @tparam FnType The type of function object to execute
 * @param func The function object to execute
 * @param waitMs Delay between retry attempts
 * @return The same as the return type of func
 */
template <typename FnType>
auto
retryOnTimeout(FnType func, size_t waitMs = DEFAULT_WAIT_BETWEEN_RETRY)
{
    static util::Logger const log{"Backend"};

    while (true) {
        try {
            return func();
        } catch (DatabaseTimeout const&) {
            LOG(log.error()) << "Database request timed out. Sleeping and retrying ... ";
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        }
    }
}

/**
 * @brief Synchronously executes the given function object inside a coroutine.
 *
 * @tparam FnType The type of function object to execute
 * @param func The function object to execute
 * @return The same as the return type of func
 */
template <typename FnType>
auto
synchronous(FnType&& func)
{
    boost::asio::io_context ctx;

    using R = typename boost::result_of<FnType(boost::asio::yield_context)>::type;
    if constexpr (!std::is_same_v<R, void>) {
        R res;
        boost::asio::spawn(ctx, [_ = boost::asio::make_work_guard(ctx), &func, &res](auto yield) {
            res = func(yield);
        });

        ctx.run();
        return res;
    } else {
        boost::asio::spawn(ctx, [_ = boost::asio::make_work_guard(ctx), &func](auto yield) { func(yield); });
        ctx.run();
    }
}

/**
 * @brief Synchronously execute the given function object and retry until no DatabaseTimeout is thrown.
 *
 * @tparam FnType The type of function object to execute
 * @param func The function object to execute
 * @return The same as the return type of func
 */
template <typename FnType>
auto
synchronousAndRetryOnTimeout(FnType&& func)
{
    return retryOnTimeout([&]() { return synchronous(func); });
}

/**
 * @brief The interface to the database used by Clio.
 */
class BackendInterface {
protected:
    mutable std::shared_mutex rngMtx_;
    std::optional<LedgerRange> range;
    LedgerCache cache_;
    std::optional<etl::CorruptionDetector<LedgerCache>> corruptionDetector_;

public:
    BackendInterface() = default;
    virtual ~BackendInterface() = default;

    // TODO: Remove this hack. Cache should not be exposed thru BackendInterface
    /**
     * @return Immutable cache
     */
    LedgerCache const&
    cache() const
    {
        return cache_;
    }

    /**
     * @return Mutable cache
     */
    LedgerCache&
    cache()
    {
        return cache_;
    }

    /**
     * @brief Sets the corruption detector.
     *
     * @param detector The corruption detector to set
     */
    void
    setCorruptionDetector(etl::CorruptionDetector<LedgerCache> detector)
    {
        corruptionDetector_ = std::move(detector);
    }

    /**
     * @brief Fetches a specific ledger by sequence number.
     *
     * @param sequence The sequence number to fetch for
     * @param yield The coroutine context
     * @return The ripple::LedgerHeader if found; nullopt otherwise
     */
    virtual std::optional<ripple::LedgerHeader>
    fetchLedgerBySequence(std::uint32_t sequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches a specific ledger by hash.
     *
     * @param hash The hash to fetch for
     * @param yield The coroutine context
     * @return The ripple::LedgerHeader if found; nullopt otherwise
     */
    virtual std::optional<ripple::LedgerHeader>
    fetchLedgerByHash(ripple::uint256 const& hash, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches the latest ledger sequence.
     *
     * @param yield The coroutine context
     * @return Latest sequence wrapped in an optional if found; nullopt otherwise
     */
    virtual std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetch the current ledger range.
     *
     * @return The current ledger range if populated; nullopt otherwise
     */
    std::optional<LedgerRange>
    fetchLedgerRange() const;

    /**
     * @brief Fetch the specified number of account root object indexes by page, the accounts need to exist for seq.
     *
     * @param number The number of accounts to fetch
     * @param pageSize The maximum number of accounts per page
     * @param seq The accounts need to exist for this sequence
     * @param yield The coroutine context
     * @return A vector of ripple::uint256 representing the account roots
     */
    virtual std::vector<ripple::uint256>
    fetchAccountRoots(std::uint32_t number, std::uint32_t pageSize, std::uint32_t seq, boost::asio::yield_context yield)
        const = 0;

    /**
     * @brief Updates the range of sequences that are stored in the DB.
     *
     * @param newMax The new maximum sequence available
     */
    void
    updateRange(uint32_t newMax);

    /**
     * @brief Sets the range of sequences that are stored in the DB.
     *
     * @param min The new minimum sequence available
     * @param max The new maximum sequence available
     * @param force If set to true, the range will be set even if it's already set
     */
    void
    setRange(uint32_t min, uint32_t max, bool force = false);

    /**
     * @brief Fetch the fees from a specific ledger sequence.
     *
     * @param seq The sequence to fetch for
     * @param yield The coroutine context
     * @return Fees if fees are found; nullopt otherwise
     */
    std::optional<ripple::Fees>
    fetchFees(std::uint32_t seq, boost::asio::yield_context yield) const;

    /**
     * @brief Fetches a specific transaction.
     *
     * @param hash The hash of the transaction to fetch
     * @param yield The coroutine context
     * @return TransactionAndMetadata if transaction is found; nullopt otherwise
     */
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches multiple transactions.
     *
     * @param hashes A vector of hashes to fetch transactions for
     * @param yield The coroutine context
     * @return A vector of TransactionAndMetadata matching the given hashes
     */
    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches all transactions for a specific account.
     *
     * @param account The account to fetch transactions for
     * @param limit The maximum number of transactions per result page
     * @param forward Whether to fetch the page forwards or backwards from the given cursor
     * @param cursor The cursor to resume fetching from
     * @param yield The coroutine context
     * @return Results and a cursor to resume from
     */
    virtual TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @brief Fetches all transactions from a specific ledger.
     *
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return Results as a vector of TransactionAndMetadata
     */
    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches all transaction hashes from a specific ledger.
     *
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return Hashes as ripple::uint256 in a vector
     */
    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches a specific NFT.
     *
     * @param tokenID The ID of the NFT
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return NFT object on success; nullopt otherwise
     */
    virtual std::optional<NFT>
    fetchNFT(ripple::uint256 const& tokenID, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches all transactions for a specific NFT.
     *
     * @param tokenID The ID of the NFT
     * @param limit The maximum number of transactions per result page
     * @param forward Whether to fetch the page forwards or backwards from the given cursor
     * @param cursorIn The cursor to resume fetching from
     * @param yield The coroutine context
     * @return Results and a cursor to resume from
     */
    virtual TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @brief Fetches all NFTs issued by a given address.
     *
     * @param issuer AccountID of issuer you wish you query.
     * @param taxon Optional taxon of NFTs by which you wish to filter.
     * @param ledgerSequence The ledger sequence to fetch for
     * @param limit Paging limit.
     * @param cursorIn Optional cursor to allow us to pick up from where we last left off.
     * @param yield Currently executing coroutine.
     * @return NFTs issued by this account, or this issuer/taxon combination if taxon is passed and an optional marker
     */
    virtual NFTsAndCursor
    fetchNFTsByIssuer(
        ripple::AccountID const& issuer,
        std::optional<std::uint32_t> const& taxon,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @brief Fetches all holders' balances for a MPTIssuanceID
     *
     * @param mptID MPTIssuanceID you wish you query.
     * @param limit Paging limit.
     * @param cursorIn Optional cursor to allow us to pick up from where we last left off.
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield Currently executing coroutine.
     * @return std::vector<Blob> of MPToken balances and an optional marker
     */
    virtual MPTHoldersAndCursor
    fetchMPTHolders(
        ripple::uint192 const& mptID,
        std::uint32_t const limit,
        std::optional<ripple::AccountID> const& cursorIn,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @brief Fetches a specific ledger object.
     *
     * Currently the real fetch happens in doFetchLedgerObject and fetchLedgerObject attempts to fetch from Cache first
     * and only calls out to the real DB if a cache miss ocurred.
     *
     * @param key The key of the object
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The object as a Blob on success; nullopt otherwise
     */
    std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, std::uint32_t sequence, boost::asio::yield_context yield) const;

    /**
     * @brief Fetches a specific ledger object sequence.
     *
     * Currently the real fetch happens in doFetchLedgerObjectSeq
     *
     * @param key The key of the object
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The sequence in unit32_t on success; nullopt otherwise
     */
    std::optional<std::uint32_t>
    fetchLedgerObjectSeq(ripple::uint256 const& key, std::uint32_t sequence, boost::asio::yield_context yield) const;

    /**
     * @brief Fetches all ledger objects by their keys.
     *
     * Currently the real fetch happens in doFetchLedgerObjects and fetchLedgerObjects attempts to fetch from Cache
     * first and only calls out to the real DB for each of the keys that was not found in the cache.
     *
     * @param keys A vector with the keys of the objects to fetch
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return A vector of ledger objects as Blobs
     */
    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t sequence,
        boost::asio::yield_context yield
    ) const;

    /**
     * @brief The database-specific implementation for fetching a ledger object.
     *
     * @param key The key to fetch for
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The object as a Blob on success; nullopt otherwise
     */
    virtual std::optional<Blob>
    doFetchLedgerObject(ripple::uint256 const& key, std::uint32_t sequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief The database-specific implementation for fetching a ledger object sequence.
     *
     * @param key The key to fetch for
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The sequence in unit32_t on success; nullopt otherwise
     */
    virtual std::optional<std::uint32_t>
    doFetchLedgerObjectSeq(ripple::uint256 const& key, std::uint32_t sequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief The database-specific implementation for fetching ledger objects.
     *
     * @param keys The keys to fetch for
     * @param sequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return A vector of Blobs representing each fetched object
     */
    virtual std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t sequence,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @brief Returns the difference between ledgers.
     *
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return A vector of LedgerObject representing the diff
     */
    virtual std::vector<LedgerObject>
    fetchLedgerDiff(std::uint32_t ledgerSequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches a page of ledger objects, ordered by key/index.
     *
     * @param cursor The cursor to resume fetching from
     * @param ledgerSequence The ledger sequence to fetch for
     * @param limit The maximum number of transactions per result page
     * @param outOfOrder If set to true max available sequence is used instead of ledgerSequence
     * @param yield The coroutine context
     * @return The ledger page
     */
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        bool outOfOrder,
        boost::asio::yield_context yield
    );

    /**
     * @brief Fetches the successor object.
     *
     * @param key The key to fetch for
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The sucessor on success; nullopt otherwise
     */
    std::optional<LedgerObject>
    fetchSuccessorObject(ripple::uint256 key, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const;

    /**
     * @brief Fetches the successor key.
     *
     * Thea real fetch happens in doFetchSuccessorKey. This function will attempt to lookup the successor in the cache
     * first and only if it's not found in the cache will it fetch from the actual DB.
     *
     * @param key The key to fetch for
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The sucessor key on success; nullopt otherwise
     */
    std::optional<ripple::uint256>
    fetchSuccessorKey(ripple::uint256 key, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const;

    /**
     * @brief Database-specific implementation of fetching the successor key
     *
     * @param key The key to fetch for
     * @param ledgerSequence The ledger sequence to fetch for
     * @param yield The coroutine context
     * @return The sucessor on success; nullopt otherwise
     */
    virtual std::optional<ripple::uint256>
    doFetchSuccessorKey(ripple::uint256 key, std::uint32_t ledgerSequence, boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches book offers.
     *
     * @param book Unsigned 256-bit integer.
     * @param ledgerSequence The ledger sequence to fetch for
     * @param limit Pagaing limit as to how many transactions returned per page.
     * @param yield The coroutine context
     * @return The book offers page
     */
    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        boost::asio::yield_context yield
    ) const;

    /**
     * @brief Synchronously fetches the ledger range from DB.
     *
     * This function just wraps hardFetchLedgerRange(boost::asio::yield_context) using synchronous(FnType&&).
     *
     * @return The ledger range if available; nullopt otherwise
     */
    std::optional<LedgerRange>
    hardFetchLedgerRange() const;

    /**
     * @brief Fetches the ledger range from DB.
     *
     * @param yield The coroutine context
     * @return The ledger range if available; nullopt otherwise
     */
    virtual std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context yield) const = 0;

    /**
     * @brief Fetches the ledger range from DB retrying until no DatabaseTimeout is thrown.
     *
     * @return The ledger range if available; nullopt otherwise
     */
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow() const;

    /**
     * @brief Writes to a specific ledger.
     *
     * @param ledgerHeader Ledger header.
     * @param blob r-value string serialization of ledger header.
     */
    virtual void
    writeLedger(ripple::LedgerHeader const& ledgerHeader, std::string&& blob) = 0;

    /**
     * @brief Writes a new ledger object.
     *
     * @param key The key to write the ledger object under
     * @param seq The ledger sequence to write for
     * @param blob The data to write
     */
    virtual void
    writeLedgerObject(std::string&& key, std::uint32_t seq, std::string&& blob);

    /**
     * @brief Writes a new transaction.
     *
     * @param hash The hash of the transaction
     * @param seq The ledger sequence to write for
     * @param date The timestamp of the entry
     * @param transaction The transaction data to write
     * @param metadata The metadata to write
     */
    virtual void
    writeTransaction(
        std::string&& hash,
        std::uint32_t seq,
        std::uint32_t date,
        std::string&& transaction,
        std::string&& metadata
    ) = 0;

    /**
     * @brief Writes NFTs to the database.
     *
     * @param data A vector of NFTsData objects representing the NFTs
     */
    virtual void
    writeNFTs(std::vector<NFTsData> const& data) = 0;

    /**
     * @brief Write a new set of account transactions.
     *
     * @param data A vector of AccountTransactionsData objects representing the account transactions
     */
    virtual void
    writeAccountTransactions(std::vector<AccountTransactionsData> data) = 0;

    /**
     * @brief Write NFTs transactions.
     *
     * @param data A vector of NFTTransactionsData objects
     */
    virtual void
    writeNFTTransactions(std::vector<NFTTransactionsData> const& data) = 0;

    /**
     * @brief Write accounts that started holding onto a MPT.
     *
     * @param data A vector of MPT ID and account pairs
     */
    virtual void
    writeMPTHolders(std::vector<MPTHolderData> const& data) = 0;

    /**
     * @brief Write a new successor.
     *
     * @param key Key of the object that the passed successor will be the successor for
     * @param seq The ledger sequence to write for
     * @param successor The successor data to write
     */
    virtual void
    writeSuccessor(std::string&& key, std::uint32_t seq, std::string&& successor) = 0;

    /**
     * @brief Starts a write transaction with the DB. No-op for cassandra.
     *
     * Note: Can potentially be deprecated and removed.
     */
    virtual void
    startWrites() const = 0;

    /**
     * @brief Tells database we finished writing all data for a specific ledger.
     *
     * Uses doFinishWrites to synchronize with the pending writes.
     *
     * @param ledgerSequence The ledger sequence to finish writing for
     * @return true on success; false otherwise
     */
    bool
    finishWrites(std::uint32_t ledgerSequence);

    /**
     * @return true if database is overwhelmed; false otherwise
     */
    virtual bool
    isTooBusy() const = 0;

    /**
     * @return A JSON object containing backend usage statistics
     */
    virtual boost::json::object
    stats() const = 0;

private:
    /**
     * @brief Writes a ledger object to the database
     *
     * @param key The key of the object
     * @param seq The sequence of the ledger
     * @param blob The data
     */
    virtual void
    doWriteLedgerObject(std::string&& key, std::uint32_t seq, std::string&& blob) = 0;

    /**
     * @brief The implementation should wait for all pending writes to finish
     *
     * @return true on success; false otherwise
     */
    virtual bool
    doFinishWrites() = 0;
};

}  // namespace data
using BackendInterface = data::BackendInterface;
