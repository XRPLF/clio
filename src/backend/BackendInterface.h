#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <boost/asio.hpp>
#include <backend/DBHelpers.h>
#include <backend/SimpleCache.h>
#include <backend/Types.h>
#include <thread>
#include <type_traits>
namespace Backend {

class DatabaseTimeout : public std::exception
{
    const char*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};

template <class F>
auto
retryOnTimeout(F func, size_t waitMs = 500)
{
    while (true)
    {
        try
        {
            return func();
        }
        catch (DatabaseTimeout& t)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
            BOOST_LOG_TRIVIAL(error)
                << __func__ << " function timed out. Retrying ... ";
        }
    }
}

template <class F>
auto
synchronous(F&& f)
{
    boost::asio::io_context ctx;
    boost::asio::io_context::strand strand(ctx);
    std::optional<boost::asio::io_context::work> work;

    work.emplace(ctx);

    using R = typename std::result_of<F(boost::asio::yield_context&)>::type;
    if constexpr (!std::is_same<R, void>::value)
    {
        R res;
        boost::asio::spawn(
            strand, [&f, &work, &res](boost::asio::yield_context yield) {
                res = f(yield);
                work.reset();
            });

        ctx.run();
        return res;
    }
    else
    {
        boost::asio::spawn(
            strand, [&f, &work](boost::asio::yield_context yield) {
                f(yield);
                work.reset();
            });

        ctx.run();
    }
}

template <class F>
auto
synchronousAndRetryOnTimeout(F&& f)
{
    return retryOnTimeout([&]() { return synchronous(f); });
}

class BackendInterface
{
protected:
    mutable std::shared_mutex rngMtx_;
    std::optional<LedgerRange> range;
    SimpleCache cache_;

    // mutex used for open() and close()
    mutable std::mutex mutex_;

public:
    BackendInterface(boost::json::object const& config)
    {
    }
    virtual ~BackendInterface()
    {
    }

    // *** public read methods ***
    // All of these reads methods can throw DatabaseTimeout. When writing code
    // in an RPC handler, this exception does not need to be caught: when an RPC
    // results in a timeout, an error is returned to the client
public:
    // *** ledger methods
    //

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

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const = 0;

    virtual std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const = 0;

    std::optional<LedgerRange>
    fetchLedgerRange() const
    {
        std::shared_lock lck(rngMtx_);
        return range;
    }

    void
    updateRange(uint32_t newMax)
    {
        std::unique_lock lck(rngMtx_);
        assert(!range || newMax >= range->maxSequence);
        if (!range)
            range = {newMax, newMax};
        else
            range->maxSequence = newMax;
    }

    std::optional<ripple::Fees>
    fetchFees(std::uint32_t const seq, boost::asio::yield_context& yield) const;

    // *** transaction methods
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const = 0;

    virtual AccountTransactions
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    // *** NFT methods
    virtual std::optional<NFToken>
    fetchNFToken(ripple::uint256 tokenID, uint32_t ledgerSequence) const = 0;

    virtual std::optional<LedgerObject>
    fetchNFTokenPage(
        ripple::uint256 ledgerKeyMin,
        ripple::uint256 ledgerKeyMax,
        uint32_t ledgerSequence) const = 0;

    // *** state data methods
    std::optional<Blob>
    fetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const;

    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const;

    virtual std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    // Fetches a page of ledger objects, ordered by key/index.
    // Used by ledger_data
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        bool outOfOrder,
        boost::asio::yield_context& yield) const;

    // Fetches the successor to key/index
    std::optional<LedgerObject>
    fetchSuccessorObject(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const;

    std::optional<ripple::uint256>
    fetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const;
    // Fetches the successor to key/index

    virtual std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursor,
        boost::asio::yield_context& yield) const;

    std::optional<LedgerRange>
    hardFetchLedgerRange() const
    {
        return synchronous([&](boost::asio::yield_context yield) {
            return hardFetchLedgerRange(yield);
        });
    }

    virtual std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const = 0;

    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow() const;
    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow(boost::asio::yield_context& yield) const;

    virtual void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader) = 0;

    virtual void
    writeLedgerObject(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& blob);

    virtual void
    writeTransaction(
        std::string&& hash,
        std::uint32_t const seq,
        std::uint32_t const date,
        std::string&& transaction,
        std::string&& metadata) = 0;

    virtual void
    writeNFTokens(std::vector<NFTokensData>&& data) = 0;

    virtual void
    writeAccountTransactions(std::vector<AccountTransactionsData>&& data) = 0;

    virtual void
    writeNFTokenTransactions(std::vector<NFTokenTransactionsData>&& data) = 0;

    virtual void
    writeSuccessor(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& successor) = 0;

    // Tell the database we are about to begin writing data for a particular
    // ledger.
    virtual void
    startWrites() const = 0;

    // Tell the database we have finished writing all data for a particular
    // ledger
    // TODO change the return value to represent different results. committed,
    // write conflict, errored, successful but not committed
    bool
    finishWrites(std::uint32_t const ledgerSequence);

    virtual bool
    doOnlineDelete(
        std::uint32_t numLedgersToKeep,
        boost::asio::yield_context& yield) const = 0;

    // Open the database. Set up all of the necessary objects and
    // datastructures. After this call completes, the database is ready for
    // use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close(){};

    // *** private helper methods
private:
    virtual void
    doWriteLedgerObject(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& blob) = 0;

    virtual bool
    doFinishWrites() = 0;
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
