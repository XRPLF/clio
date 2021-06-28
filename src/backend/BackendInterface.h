#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <boost/asio.hpp>
#include <backend/BackendIndexer.h>
#include <backend/DBHelpers.h>
class ReportingETL;
class AsyncCallData;
class BackendTest_Basic_Test;
namespace Backend {

// *** return types

using Blob = std::vector<unsigned char>;

struct LedgerObject
{
    ripple::uint256 key;
    Blob blob;
};

struct LedgerPage
{
    std::vector<LedgerObject> objects;
    std::optional<ripple::uint256> cursor;
    std::optional<std::string> warning;
};
struct BookOffersPage
{
    std::vector<LedgerObject> offers;
    std::optional<ripple::uint256> cursor;
    std::optional<std::string> warning;
};
struct TransactionAndMetadata
{
    Blob transaction;
    Blob metadata;
    uint32_t ledgerSequence;
    bool
    operator==(const TransactionAndMetadata&) const = default;
};

struct AccountTransactionsCursor
{
    uint32_t ledgerSequence;
    uint32_t transactionIndex;
};

struct LedgerRange
{
    uint32_t minSequence;
    uint32_t maxSequence;
};

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
    mutable BackendIndexer indexer_;
    mutable bool isFirst_ = true;

public:
    BackendInterface(boost::json::object const& config) : indexer_(config)
    {
    }
    virtual ~BackendInterface()
    {
    }

    BackendIndexer&
    getIndexer() const
    {
        return indexer_;
    }

    // *** public read methods ***
    // All of these reads methods can throw DatabaseTimeout. When writing code
    // in an RPC handler, this exception does not need to be caught: when an RPC
    // results in a timeout, an error is returned to the client
public:
    // *** ledger methods

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence() const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash) const = 0;

    virtual std::optional<LedgerRange>
    fetchLedgerRange() const = 0;

    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    fetchLedgerRangeNoThrow() const;

    // *** transaction methods

    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes) const = 0;

    virtual std::pair<
        std::vector<TransactionAndMetadata>,
        std::optional<AccountTransactionsCursor>>
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        std::optional<AccountTransactionsCursor> const& cursor = {}) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const = 0;

    // *** state data methods

    virtual std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence) const = 0;

    virtual std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const = 0;

    // Fetches a page of ledger objects, ordered by key/index.
    // Used by ledger_data
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        std::uint32_t limitHint = 0) const;

    // Fetches the successor to key/index. key need not actually be a valid
    // key/index.
    std::optional<LedgerObject>
    fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence) const;

    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor = {}) const;

    // Methods related to the indexer
    bool
    isLedgerIndexed(std::uint32_t ledgerSequence) const;

    std::optional<KeyIndex>
    getKeyIndexOfSeq(uint32_t seq) const;

    // *** protected write methods
protected:
    friend class ::ReportingETL;
    friend class BackendIndexer;
    friend class ::AsyncCallData;
    friend std::shared_ptr<BackendInterface>
    make_Backend(boost::json::object const& config);
    friend class ::BackendTest_Basic_Test;

    virtual void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        bool isFirst = false) const = 0;

    void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const;

    virtual void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        std::string&& transaction,
        std::string&& metadata) const = 0;

    virtual void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) const = 0;

    // TODO: this function, or something similar, could be called internally by
    // writeLedgerObject
    virtual bool
    writeKeys(
        std::unordered_set<ripple::uint256> const& keys,
        KeyIndex const& index,
        bool isAsync = false) const = 0;

    // Tell the database we are about to begin writing data for a particular
    // ledger.
    virtual void
    startWrites() const = 0;

    // Tell the database we have finished writing all data for a particular
    // ledger
    bool
    finishWrites(uint32_t ledgerSequence) const;

    virtual bool
    doOnlineDelete(uint32_t numLedgersToKeep) const = 0;

    // Open the database. Set up all of the necessary objects and
    // datastructures. After this call completes, the database is ready for
    // use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close() = 0;

    // *** private helper methods
private:
    virtual LedgerPage
    doFetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const = 0;

    virtual void
    doWriteLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const = 0;

    virtual bool
    doFinishWrites() const = 0;

    void
    checkFlagLedgers() const;
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
