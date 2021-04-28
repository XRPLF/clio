#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <reporting/DBHelpers.h>
namespace Backend {
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
};
struct TransactionAndMetadata
{
    Blob transaction;
    Blob metadata;
    uint32_t ledgerSequence;
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
public:
    // read methods

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence() const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<LedgerRange>
    fetchLedgerRange() const = 0;

    virtual std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence) const = 0;

    // returns a transaction, metadata pair
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const = 0;

    virtual LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const = 0;

    virtual std::pair<std::vector<LedgerObject>, std::optional<ripple::uint256>>
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor = {}) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes) const = 0;

    virtual std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const = 0;

    virtual std::pair<
        std::vector<TransactionAndMetadata>,
        std::optional<AccountTransactionsCursor>>
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        std::optional<AccountTransactionsCursor> const& cursor = {}) const = 0;

    // write methods

    virtual void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        bool isFirst = false) const = 0;

    virtual void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const = 0;

    virtual void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        std::string&& transaction,
        std::string&& metadata) const = 0;

    virtual void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) const = 0;

    // other database methods

    // Open the database. Set up all of the necessary objects and
    // datastructures. After this call completes, the database is ready for use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close() = 0;

    virtual void
    startWrites() const = 0;

    virtual bool
    finishWrites() const = 0;

    virtual bool
    doOnlineDelete(uint32_t minLedgerToKeep) const = 0;

    virtual ~BackendInterface()
    {
    }
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
