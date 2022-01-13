#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <boost/asio.hpp>
#include <backend/DBHelpers.h>
#include <backend/SimpleCache.h>
#include <backend/Types.h>
namespace Backend {

class DatabaseTimeout : public std::exception
{
    const char*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};

class BackendInterface
{
protected:
    std::optional<LedgerRange> range;
    SimpleCache cache_;
    boost::asio::io_context& ioContext_;
    
    // mutex used for open() and close()
    mutable std::mutex mutex_;


public:
    BackendInterface(
        boost::asio::io_context& ioc,
        boost::json::object const& config) : ioContext_(ioc)
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

    boost::asio::io_context&
    getIOContext() const
    {
        return ioContext_;
    }

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        uint32_t sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const = 0;

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const = 0;

    std::optional<LedgerRange>
    fetchLedgerRange() const
    {
        std::lock_guard lk(mutex_);
        return range;
    }

    std::optional<ripple::Fees>
    fetchFees(
        std::uint32_t seq,
        boost::asio::yield_context& yield) const;

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
        std::uint32_t limit,
        bool forward,
        std::optional<AccountTransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    // *** state data methods
    std::optional<Blob>
    fetchLedgerObject(
        ripple::uint256 const& key,
        uint32_t sequence,
        boost::asio::yield_context& yield) const;

    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence,
        boost::asio::yield_context& yield) const;

    virtual std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        uint32_t sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence,
        boost::asio::yield_context& yield) const = 0;

    virtual std::vector<LedgerObject>
    fetchLedgerDiff(
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    // Fetches a page of ledger objects, ordered by key/index.
    // Used by ledger_data
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        std::uint32_t limitHint,
        boost::asio::yield_context& yield) const;

    // Fetches the successor to key/index
    std::optional<LedgerObject>
    fetchSuccessorObject(
        ripple::uint256 key,
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const;

    std::optional<ripple::uint256>
    fetchSuccessorKey(
        ripple::uint256 key,
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const;
    // Fetches the successor to key/index

    virtual std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const = 0;

    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor,
        boost::asio::yield_context& yield) const;


    virtual std::optional<LedgerRange>
    hardFetchLedgerRange() const = 0;
    virtual std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const = 0;

    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow() const;
    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow(boost::asio::yield_context& yield) const;

    void
    updateRange(uint32_t newMax)
    {
        std::lock_guard lk(mutex_);
        if (!range)
            range = {newMax, newMax};
        else
            range->maxSequence = newMax;
    }

    virtual void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        boost::asio::yield_context& yield) = 0;

    virtual void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        boost::asio::yield_context& yield);

    virtual void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        uint32_t date,
        std::string&& transaction,
        std::string&& metadata,
        boost::asio::yield_context& yield) = 0;

    virtual void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data,
        boost::asio::yield_context& yield) = 0;

    virtual void
    writeSuccessor(
        std::string&& key,
        uint32_t seq,
        std::string&& successor,
        boost::asio::yield_context& yield) = 0;
    // Tell the database we are about to begin writing data for a particular
    // ledger.
    virtual void
    startWrites(boost::asio::yield_context& yield) const = 0;

    // Tell the database we have finished writing all data for a particular
    // ledger
    bool    
    finishWrites(uint32_t ledgerSequence, boost::asio::yield_context& yield);

    virtual bool
    doOnlineDelete(
        uint32_t numLedgersToKeep,
        boost::asio::yield_context& yield) const = 0;

    // Open the database. Set up all of the necessary objects and
    // datastructures. After this call completes, the database is ready for
    // use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close()
    {
    };

    // *** private helper methods
private:
    virtual void
    doWriteLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        boost::asio::yield_context& yield) = 0;

    virtual bool
    doFinishWrites(boost::asio::yield_context& yield) const = 0;
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
