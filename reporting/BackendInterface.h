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
class BackendInterface;
class BackendIndexer
{
    boost::asio::io_context ioc_;
    std::mutex mutex_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;
    uint32_t keyShift_ = 20;
    uint32_t bookShift_ = 10;
    std::unordered_set<ripple::uint256> keys;
    std::unordered_set<ripple::uint256> keysCumulative;
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
        books;
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
        booksCumulative;
    bool populatingCacheAsync = false;
    // These are only used when the cache is being populated asynchronously
    std::unordered_set<ripple::uint256> deletedKeys;
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
        deletedBooks;
    std::unordered_set<ripple::uint256> keysRepair;
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
        booksRepair;
    std::mutex mtx;
    std::condition_variable cv_;

    void
    addKeyAsync(ripple::uint256 const& key);
    void
    addBookOfferAsync(
        ripple::uint256 const& book,
        ripple::uint256 const& offerKey);

public:
    BackendIndexer(boost::json::object const& config);
    ~BackendIndexer();

    void
    populateCachesAsync(BackendInterface const& backend);
    void
    populateCaches(BackendInterface const& backend);
    void
    clearCaches();
    // Blocking, possibly for minutes
    void
    waitForCaches();

    void
    addKey(ripple::uint256 const& key);
    void
    deleteKey(ripple::uint256 const& key);
    
    void
    addBookOffer(ripple::uint256 const& book, ripple::uint256 const& offerKey);

    void
    deleteBookOffer(
        ripple::uint256 const& book,
        ripple::uint256 const& offerKey);

    void
    finish(uint32_t ledgerSequence, BackendInterface const& backend);
    void
    writeKeyFlagLedgerAsync(
        uint32_t ledgerSequence,
        BackendInterface const& backend);
    void
    writeBookFlagLedgerAsync(
        uint32_t ledgerSequence,
        BackendInterface const& backend);
    void
    doKeysRepair(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    void
    doBooksRepair(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    uint32_t
    getBookShift()
    {
        return bookShift_;
    }
    uint32_t
    getKeyShift()
    {
        return keyShift_;
    }
    uint32_t
    getKeyIndexOfSeq(uint32_t seq) const
    {
        if (isKeyFlagLedger(seq))
            return seq;
        auto incr = (1 << keyShift_);
        return (seq >> keyShift_ << keyShift_) + incr;
    }
    bool
    isKeyFlagLedger(uint32_t ledgerSequence) const
    {
        return (ledgerSequence % (1 << keyShift_)) == 0;
    }
    uint32_t
    getBookIndexOfSeq(uint32_t seq) const
    {
        if (isBookFlagLedger(seq))
            return seq;
        auto incr = (1 << bookShift_);
        return (seq >> bookShift_ << bookShift_) + incr;
    }
    bool
    isBookFlagLedger(uint32_t ledgerSequence) const
    {
        return (ledgerSequence % (1 << bookShift_)) == 0;
    }
};

class BackendInterface
{
protected:
    mutable BackendIndexer indexer_;

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

    std::optional<uint32_t>
    getKeyIndexOfSeq(uint32_t seq) const
    {
        if (indexer_.isKeyFlagLedger(seq))
            return seq;
        auto rng = fetchLedgerRange();
        if (!rng)
            return {};
        if (rng->minSequence == seq)
            return seq;
        return indexer_.getKeyIndexOfSeq(seq);
    }
    std::optional<uint32_t>
    getBookIndexOfSeq(uint32_t seq) const
    {
        if (indexer_.isBookFlagLedger(seq))
            return seq;
        auto rng = fetchLedgerRange();
        if (!rng)
            return {};
        if (rng->minSequence == seq)
            return seq;
        return indexer_.getBookIndexOfSeq(seq);
    }

    bool
    finishWrites(uint32_t ledgerSequence) const
    {
        indexer_.finish(ledgerSequence, *this);
        auto commitRes = doFinishWrites();
        if (commitRes)
        {
            if (indexer_.isBookFlagLedger(ledgerSequence))
                indexer_.writeBookFlagLedgerAsync(ledgerSequence, *this);
            if (indexer_.isKeyFlagLedger(ledgerSequence))
                indexer_.writeKeyFlagLedgerAsync(ledgerSequence, *this);
        }
        return commitRes;
    }

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence() const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<LedgerRange>
    fetchLedgerRange() const = 0;

    std::optional<LedgerRange>
    fetchLedgerRangeNoThrow() const
    {
        while (true)
        {
            try
            {
                return fetchLedgerRange();
            }
            catch (DatabaseTimeout& t)
            {
                ;
            }
        }
    }

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

    // TODO add warning for incomplete data
    virtual BookOffersPage
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

    void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const
    {
        ripple::uint256 key256 = ripple::uint256::fromVoid(key.data());
        if (isCreated)
            indexer_.addKey(key256);
        if (isDeleted)
            indexer_.deleteKey(key256);
        if (book)
        {
            if (isCreated)
                indexer_.addBookOffer(*book, key256);
            if (isDeleted)
                indexer_.deleteBookOffer(*book, key256);
        }
        doWriteLedgerObject(
            std::move(key),
            seq,
            std::move(blob),
            isCreated,
            isDeleted,
            std::move(book));
    }
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
    doOnlineDelete(uint32_t minLedgerToKeep) const = 0;
    virtual bool
    writeKeys(
        std::unordered_set<ripple::uint256> const& keys,
        uint32_t ledgerSequence,
        bool isAsync = false) const = 0;
    virtual bool
    writeBooks(
        std::unordered_map<
            ripple::uint256,
            std::unordered_set<ripple::uint256>> const& books,
        uint32_t ledgerSequence,
        bool isAsync = false) const = 0;

    virtual ~BackendInterface()
    {
    }
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
