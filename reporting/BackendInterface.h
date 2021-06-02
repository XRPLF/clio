#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <boost/asio.hpp>
#include <reporting/DBHelpers.h>
namespace std {
template <>
struct hash<ripple::uint256>
{
    std::size_t
    operator()(const ripple::uint256& k) const noexcept
    {
        return boost::hash_range(k.begin(), k.end());
    }
};
}  // namespace std
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

// The below two structs exist to prevent developers from accidentally mixing up
// the two indexes.
struct BookIndex
{
    uint32_t bookIndex;
    explicit BookIndex(uint32_t v) : bookIndex(v){};
};
struct KeyIndex
{
    uint32_t keyIndex;
    explicit KeyIndex(uint32_t v) : keyIndex(v){};
};

class DatabaseTimeout : public std::exception
{
    const char*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};
class BackendInterface;
class BackendIndexer
{
    boost::asio::io_context ioc_;
    boost::asio::io_context::strand strand_;
    std::mutex mutex_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    std::atomic_uint32_t indexing_ = 0;

    uint32_t keyShift_ = 20;
    std::unordered_set<ripple::uint256> keys;

    mutable bool isFirst_ = true;
    void
    doKeysRepair(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    void
    writeKeyFlagLedger(
        uint32_t ledgerSequence,
        BackendInterface const& backend);

public:
    BackendIndexer(boost::json::object const& config);
    ~BackendIndexer();

    void
    addKey(ripple::uint256&& key);

    void
    finish(uint32_t ledgerSequence, BackendInterface const& backend);
    void
    writeKeyFlagLedgerAsync(
        uint32_t ledgerSequence,
        BackendInterface const& backend);
    void
    doKeysRepairAsync(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    uint32_t
    getKeyShift()
    {
        return keyShift_;
    }
    std::optional<uint32_t>
    getCurrentlyIndexing()
    {
        uint32_t cur = indexing_.load();
        if (cur != 0)
            return cur;
        return {};
    }
    KeyIndex
    getKeyIndexOfSeq(uint32_t seq) const
    {
        if (isKeyFlagLedger(seq))
            return KeyIndex{seq};
        auto incr = (1 << keyShift_);
        KeyIndex index{(seq >> keyShift_ << keyShift_) + incr};
        assert(isKeyFlagLedger(index.keyIndex));
        return index;
    }
    bool
    isKeyFlagLedger(uint32_t ledgerSequence) const
    {
        return (ledgerSequence % (1 << keyShift_)) == 0;
    }
};

class BackendInterface
{
protected:
    mutable BackendIndexer indexer_;
    mutable bool isFirst_ = true;

public:
    // read methods
    BackendInterface(boost::json::object const& config) : indexer_(config)
    {
    }

    BackendIndexer&
    getIndexer() const
    {
        return indexer_;
    }

    void
    checkFlagLedgers() const;

    std::optional<KeyIndex>
    getKeyIndexOfSeq(uint32_t seq) const;

    bool
    finishWrites(uint32_t ledgerSequence) const;

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence() const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<LedgerRange>
    fetchLedgerRange() const = 0;

    std::optional<LedgerRange>
    fetchLedgerRangeNoThrow() const;

    virtual std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence) const = 0;

    // returns a transaction, metadata pair
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const = 0;

    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const;

    bool
    isLedgerIndexed(std::uint32_t ledgerSequence) const;

    std::optional<LedgerObject>
    fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence) const;

    virtual LedgerPage
    doFetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const = 0;

    // TODO add warning for incomplete data
    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor = {}) const;

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

    void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const;

    virtual void
    doWriteLedgerObject(
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
    // datastructures. After this call completes, the database is ready for
    // use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close() = 0;

    virtual void
    startWrites() const = 0;

    virtual bool
    doFinishWrites() const = 0;

    virtual bool
    doOnlineDelete(uint32_t numLedgersToKeep) const = 0;
    virtual bool
    writeKeys(
        std::unordered_set<ripple::uint256> const& keys,
        KeyIndex const& index,
        bool isAsync = false) const = 0;

    virtual ~BackendInterface()
    {
    }
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
