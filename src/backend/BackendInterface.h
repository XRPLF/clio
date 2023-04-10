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

#include <ripple/ledger/ReadView.h>
#include <backend/DBHelpers.h>
#include <backend/SimpleCache.h>
#include <backend/Types.h>
#include <config/Config.h>
#include <log/Logger.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <thread>
#include <type_traits>

namespace Backend {

/**
 * @brief Throws an error when database read time limit is exceeded.
 *
 * This class is throws an error when read time limit is exceeded but
 * is also paired with a separate class to retry the connection.
 */
class DatabaseTimeout : public std::exception
{
public:
    const char*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};

/**
 * @brief Separate class that reattempts connection after time limit.
 *
 * @tparam F Represents a class of handlers for Cassandra database.
 * @param func Instance of Cassandra database handler class.
 * @param waitMs Is the arbitrary time limit of 500ms.
 * @return auto
 */
template <class F>
auto
retryOnTimeout(F func, size_t waitMs = 500)
{
    static clio::Logger log{"Backend"};

    while (true)
    {
        try
        {
            return func();
        }
        catch (DatabaseTimeout& t)
        {
            log.error() << "Database request timed out. Sleeping and retrying ... ";
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        }
    }
}

/**
 * @brief Passes in serialized handlers in an asynchronous fashion.
 *
 * Note that the synchronous auto passes handlers critical to supporting
 * the Clio backend. The coroutine types are checked if same/different.
 *
 * @tparam F Represents a class of handlers for Cassandra database.
 * @param f R-value instance of Cassandra handler class.
 * @return auto
 */
template <class F>
auto
synchronous(F&& f)
{
    /** @brief Serialized handlers and their execution.
     *
     * The ctx class is converted into a serialized handler, also named
     * ctx, and is used to pass a stream of data into the method.
     */
    boost::asio::io_context ctx;
    boost::asio::io_context::strand strand(ctx);
    std::optional<boost::asio::io_context::work> work;

    /*! @brief Place the ctx within the vector of serialized handlers. */
    work.emplace(ctx);

    /**
     * @brief If/else statements regarding coroutine type matching.
     *
     * R is the currently executing coroutine that is about to get passed.
     * If corountine types do not match, the current one's type is stored.
     */
    using R = typename boost::result_of<F(boost::asio::yield_context&)>::type;
    if constexpr (!std::is_same<R, void>::value)
    {
        /**
         * @brief When the coroutine type is the same
         *
         * The spawn function enables programs to implement asynchronous logic
         * in a synchronous manner. res stores the instance of the currently
         * executing coroutine, yield. The different type is returned.
         */
        R res;
        boost::asio::spawn(strand, [&f, &work, &res](boost::asio::yield_context yield) {
            res = f(yield);
            work.reset();
        });

        ctx.run();
        return res;
    }
    else
    {
        /*! @brief When the corutine type is different, run as normal. */
        boost::asio::spawn(strand, [&f, &work](boost::asio::yield_context yield) {
            f(yield);
            work.reset();
        });

        ctx.run();
    }
}

/**
 * @brief Reestablishes synchronous connection on timeout.
 *
 * @tparam Represents a class of handlers for Cassandra database.
 * @param f R-value instance of Cassandra database handler class.
 * @return auto
 */
template <class F>
auto
synchronousAndRetryOnTimeout(F&& f)
{
    return retryOnTimeout([&]() { return synchronous(f); });
}

/*! @brief Handles ledger and transaction backend data. */
class BackendInterface
{
    /**
     * @brief Shared mutexes and a cache for the interface.
     *
     * rngMutex is a shared mutex. Shared mutexes prevent shared data
     * from being accessed by multiple threads and has two levels of
     * access: shared and exclusive.
     */
protected:
    mutable std::shared_mutex rngMtx_;
    std::optional<LedgerRange> range;
    SimpleCache cache_;

    /**
     * @brief Public read methods
     *
     * All of these reads methods can throw DatabaseTimeout. When writing
     * code in an RPC handler, this exception does not need to be caught:
     * when an RPC results in a timeout, an error is returned to the client.
     */

public:
    BackendInterface() = default;
    virtual ~BackendInterface() = default;

    /*! @brief LEDGER METHODS */
public:
    /**
     * @brief Cache that holds states of the ledger
     *
     * const version holds the original cache state; the other tracks
     * historical changes.
     *
     * @return SimpleCache const&
     */
    SimpleCache const&
    cache() const
    {
        return cache_;
    }

    SimpleCache&
    cache()
    {
        return cache_;
    }

    /*! @brief Fetches a specific ledger by sequence number. */
    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(std::uint32_t const sequence, boost::asio::yield_context& yield) const = 0;

    /*! @brief Fetches a specific ledger by hash. */
    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash, boost::asio::yield_context& yield) const = 0;

    /*! @brief Fetches the latest ledger sequence. */
    virtual std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const = 0;

    /*! @brief Fetches the current ledger range while locking that process */
    std::optional<LedgerRange>
    fetchLedgerRange() const
    {
        std::shared_lock lck(rngMtx_);
        return range;
    }

    /**
     * @brief Updates the range of sequences to be tracked.
     *
     * Function that continues updating the range sliding window or creates
     * a new sliding window once the maxSequence limit has been reached.
     *
     * @param newMax Unsigned 32-bit integer representing new max of range.
     */
    void
    updateRange(uint32_t newMax)
    {
        std::scoped_lock lck(rngMtx_);
        assert(!range || newMax >= range->maxSequence);
        if (!range)
            range = {newMax, newMax};
        else
            range->maxSequence = newMax;
    }

    /**
     * @brief Returns the fees for specific transactions.
     *
     * @param seq Unsigned 32-bit integer reprsenting sequence.
     * @param yield The currently executing coroutine.
     * @return std::optional<ripple::Fees>
     */
    std::optional<ripple::Fees>
    fetchFees(std::uint32_t const seq, boost::asio::yield_context& yield) const;

    /*! @brief TRANSACTION METHODS */
    /**
     * @brief Fetches a specific transaction.
     *
     * @param hash Unsigned 256-bit integer representing hash.
     * @param yield The currently executing coroutine.
     * @return std::optional<TransactionAndMetadata>
     */
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash, boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches multiple transactions.
     *
     * @param hashes Unsigned integer value representing a hash.
     * @param yield The currently executing coroutine.
     * @return std::vector<TransactionAndMetadata>
     */
    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes, boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches all transactions for a specific account
     *
     * @param account A specific XRPL Account, speciifed by unique type
     * accountID.
     * @param limit Paging limit for how many transactions can be returned per
     * page.
     * @param forward Boolean whether paging happens forwards or backwards.
     * @param cursor Important metadata returned every time paging occurs.
     * @param yield Currently executing coroutine.
     * @return TransactionsAndCursor
     */
    virtual TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches all transactions from a specific ledger.
     *
     * @param ledgerSequence Unsigned 32-bit integer for latest total
     * transactions.
     * @param yield Currently executing coroutine.
     * @return std::vector<TransactionAndMetadata>
     */
    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches all transaction hashes from a specific ledger.
     *
     * @param ledgerSequence Standard unsigned integer.
     * @param yield Currently executing coroutine.
     * @return std::vector<ripple::uint256>
     */
    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const = 0;

    /*! @brief NFT methods */
    /**
     * @brief Fetches a specific NFT
     *
     * @param tokenID Unsigned 256-bit integer.
     * @param ledgerSequence Standard unsigned integer.
     * @param yield Currently executing coroutine.
     * @return std::optional<NFT>
     */
    virtual std::optional<NFT>
    fetchNFT(ripple::uint256 const& tokenID, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
        const = 0;

    /**
     * @brief Fetches all transactions for a specific NFT.
     *
     * @param tokenID Unsigned 256-bit integer.
     * @param limit Paging limit as to how many transactions return per page.
     * @param forward Boolean whether paging happens forwards or backwards.
     * @param cursorIn Represents transaction number and ledger sequence.
     * @param yield Currently executing coroutine is passed in as input.
     * @return TransactionsAndCursor
     */
    virtual TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t const limit,
        bool const forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches all NFTs issued by a given address.
     *
     * @param issuer AccountID of issuer you wish you query.
     * @param taxon Optional taxon of NFTs by which you wish to filter.
     * @param limit Paging limit.
     * @param cursorIn Optional cursor to allow us to pick up from where we
     * last left off.
     * @param yield Currently executing coroutine.
     * @return std::vector<NFT> of NFTs issued by this account, or
     * this issuer/taxon combination if taxon is passed and an optional marker
     */
    virtual NFTsAndCursor
    fetchNFTsByIssuer(
        ripple::AccountID const& issuer,
        std::optional<std::uint32_t> const& taxon,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context& yield) const = 0;

    /*! @brief STATE DATA METHODS */
    /**
     * @brief Fetches a specific ledger object: vector of unsigned chars
     *
     * @param key Unsigned 256-bit integer.
     * @param sequence Unsigned 32-bit integer.
     * @param yield Currently executing coroutine.
     * @return std::optional<Blob>
     */
    std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, std::uint32_t const sequence, boost::asio::yield_context& yield)
        const;

    /**
     * @brief Fetches all ledger objects: a vector of vectors of unsigned chars.
     *
     * @param keys Unsigned 256-bit integer.
     * @param sequence Unsigned 32-bit integer.
     * @param yield Currently executing coroutine.
     * @return std::vector<Blob>
     */
    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const;

    /*! @brief Virtual function version of fetchLedgerObject */
    virtual std::optional<Blob>
    doFetchLedgerObject(ripple::uint256 const& key, std::uint32_t const sequence, boost::asio::yield_context& yield)
        const = 0;

    /*! @brief Virtual function version of fetchLedgerObjects */
    virtual std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Returns the difference between ledgers: vector of objects
     *
     * Objects are made of a key value, vector of unsigned chars (blob),
     * and a boolean detailing whether keys and blob match.
     *
     * @param ledgerSequence Standard unsigned integer.
     * @param yield Currently executing coroutine.
     * @return std::vector<LedgerObject>
     */
    virtual std::vector<LedgerObject>
    fetchLedgerDiff(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Fetches a page of ledger objects, ordered by key/index.
     *
     * @param cursor Important metadata returned every time paging occurs.
     * @param ledgerSequence Standard unsigned integer.
     * @param limit Paging limit as to how many transactions returned per page.
     * @param outOfOrder Boolean on whether ledger page is out of order.
     * @param yield Currently executing coroutine.
     * @return LedgerPage
     */
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        bool outOfOrder,
        boost::asio::yield_context& yield) const;

    /*! @brief Fetches successor object from key/index. */
    std::optional<LedgerObject>
    fetchSuccessorObject(ripple::uint256 key, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
        const;

    /*! @brief Fetches successor key from key/index. */
    std::optional<ripple::uint256>
    fetchSuccessorKey(ripple::uint256 key, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const;

    /*! @brief Virtual function version of fetchSuccessorKey. */
    virtual std::optional<ripple::uint256>
    doFetchSuccessorKey(ripple::uint256 key, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
        const = 0;

    /**
     * @brief Fetches book offers.
     *
     * @param book Unsigned 256-bit integer.
     * @param ledgerSequence Standard unsigned integer.
     * @param limit Pagaing limit as to how many transactions returned per page.
     * @param cursor Important metadata returned every time paging occurs.
     * @param yield Currently executing coroutine.
     * @return BookOffersPage
     */
    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        boost::asio::yield_context& yield) const;

    /**
     * @brief Returns a ledger range
     *
     * Ledger range is a struct of min and max sequence numbers). Due to
     * the use of [&], which denotes a special case of a lambda expression
     * where values found outside the scope are passed by reference, wrt the
     * currently executing coroutine.
     *
     * @return std::optional<LedgerRange>
     */
    std::optional<LedgerRange>
    hardFetchLedgerRange() const
    {
        return synchronous([&](boost::asio::yield_context yield) { return hardFetchLedgerRange(yield); });
    }

    /*! @brief Virtual function equivalent of hardFetchLedgerRange. */
    virtual std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const = 0;

    /*! @brief Fetches ledger range but doesn't throw timeout. Use with care. */
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow() const;
    /*! @brief Fetches ledger range but doesn't throw timeout. Use with care. */
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow(boost::asio::yield_context& yield) const;

    /**
     * @brief Writes to a specific ledger.
     *
     * @param ledgerInfo Const on ledger information.
     * @param ledgerHeader r-value string representing ledger header.
     */
    virtual void
    writeLedger(ripple::LedgerInfo const& ledgerInfo, std::string&& ledgerHeader) = 0;

    /**
     * @brief Writes a new ledger object.
     *
     * The key and blob are r-value references and do NOT have memory addresses.
     *
     * @param key String represented as an r-value.
     * @param seq Unsigned integer representing a sequence.
     * @param blob r-value vector of unsigned characters (blob).
     */
    virtual void
    writeLedgerObject(std::string&& key, std::uint32_t const seq, std::string&& blob);

    /**
     * @brief Writes a new transaction.
     *
     * @param hash r-value reference. No memory address.
     * @param seq Unsigned 32-bit integer.
     * @param date Unsigned 32-bit integer.
     * @param transaction r-value reference. No memory address.
     * @param metadata r-value refrence. No memory address.
     */
    virtual void
    writeTransaction(
        std::string&& hash,
        std::uint32_t const seq,
        std::uint32_t const date,
        std::string&& transaction,
        std::string&& metadata) = 0;

    /**
     * @brief Write a new NFT.
     *
     * @param data Passed in as an r-value reference.
     */
    virtual void
    writeNFTs(std::vector<NFTsData>&& data) = 0;

    /**
     * @brief Write a new set of account transactions.
     *
     * @param data Passed in as an r-value reference.
     */
    virtual void
    writeAccountTransactions(std::vector<AccountTransactionsData>&& data) = 0;

    /**
     * @brief Write a new transaction for a specific NFT.
     *
     * @param data Passed in as an r-value reference.
     */
    virtual void
    writeNFTTransactions(std::vector<NFTTransactionsData>&& data) = 0;

    /**
     * @brief Write a new successor.
     *
     * @param key Passed in as an r-value reference.
     * @param seq Unsigned 32-bit integer.
     * @param successor Passed in as an r-value reference.
     */
    virtual void
    writeSuccessor(std::string&& key, std::uint32_t const seq, std::string&& successor) = 0;

    /*! @brief Tells database we will write data for a specific ledger. */
    virtual void
    startWrites() const = 0;

    /**
     * @brief Tells database we finished writing all data for a specific ledger.
     *
     * TODO: change the return value to represent different results:
     * Committed, write conflict, errored, successful but not committed
     *
     * @param ledgerSequence Const unsigned 32-bit integer on ledger sequence.
     * @return true
     * @return false
     */
    bool
    finishWrites(std::uint32_t const ledgerSequence);

    /**
     * @brief Selectively delets parts of the database.
     *
     * @param numLedgersToKeep Unsigned 32-bit integer on number of ledgers to
     * keep.
     * @param yield Currently executing coroutine.
     * @return true
     * @return false
     */
    virtual bool
    doOnlineDelete(std::uint32_t numLedgersToKeep, boost::asio::yield_context& yield) const = 0;

    /**
     * @brief Opens the database
     *
     * Open the database. Set up all of the necessary objects and
     * datastructures. After this call completes, the database is
     * ready for use.
     *
     * @param readOnly Boolean whether ledger is read only.
     */
    virtual void
    open(bool readOnly) = 0;

    /*! @brief Closes the database, releasing any resources. */
    virtual void
    close(){};

    virtual bool
    isTooBusy() const = 0;

private:
    /**
     * @brief Private helper method to write ledger object
     *
     * @param key r-value string representing key.
     * @param seq Unsigned 32-bit integer representing sequence.
     * @param blob r-value vector of unsigned chars.
     */
    virtual void
    doWriteLedgerObject(std::string&& key, std::uint32_t const seq, std::string&& blob) = 0;

    virtual bool
    doFinishWrites() = 0;
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
