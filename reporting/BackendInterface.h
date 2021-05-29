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
    std::mutex mutex_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    uint32_t keyShift_ = 20;
    std::unordered_set<ripple::uint256> keys;

    mutable bool isFirst_ = true;
    void
    doKeysRepair(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);

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

    std::optional<KeyIndex>
    getKeyIndexOfSeq(uint32_t seq) const
    {
        if (indexer_.isKeyFlagLedger(seq))
            return KeyIndex{seq};
        auto rng = fetchLedgerRange();
        if (!rng)
            return {};
        if (rng->minSequence == seq)
            return KeyIndex{seq};
        return indexer_.getKeyIndexOfSeq(seq);
    }

    bool
    finishWrites(uint32_t ledgerSequence) const
    {
        indexer_.finish(ledgerSequence, *this);
        auto commitRes = doFinishWrites();
        if (commitRes)
        {
            if (isFirst_)
            {
                auto rng = fetchLedgerRangeNoThrow();
                if (rng && rng->minSequence != ledgerSequence)
                    isFirst_ = false;
                indexer_.doKeysRepairAsync(*this, ledgerSequence);
            }
            if (indexer_.isKeyFlagLedger(ledgerSequence) || isFirst_)
                indexer_.writeKeyFlagLedgerAsync(ledgerSequence, *this);
            isFirst_ = false;
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
        BOOST_LOG_TRIVIAL(warning) << __func__;
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

    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const
    {
        assert(limit != 0);
        bool incomplete = false;
        {
            auto check = doFetchLedgerPage({}, ledgerSequence, 1);
            incomplete = check.warning.has_value();
        }
        uint32_t adjustedLimit = limit;
        LedgerPage page;
        page.cursor = cursor;
        do
        {
            adjustedLimit = adjustedLimit > 2048 ? 2048 : adjustedLimit * 2;
            auto partial =
                doFetchLedgerPage(page.cursor, ledgerSequence, adjustedLimit);
            page.objects.insert(
                page.objects.end(),
                partial.objects.begin(),
                partial.objects.end());
            page.cursor = partial.cursor;
        } while (page.objects.size() < limit && page.cursor);
        if (incomplete)
        {
            auto rng = fetchLedgerRange();
            if (!rng)
                return page;
            if (rng->minSequence == ledgerSequence)
            {
                BOOST_LOG_TRIVIAL(fatal)
                    << __func__
                    << " Database is populated but first flag ledger is "
                       "incomplete. This should never happen";
                assert(false);
                throw std::runtime_error("Missing base flag ledger");
            }
            BOOST_LOG_TRIVIAL(debug) << __func__ << " recursing";
            uint32_t lowerSequence = ledgerSequence >> indexer_.getKeyShift()
                    << indexer_.getKeyShift();
            if (lowerSequence < rng->minSequence)
                lowerSequence = rng->minSequence;
            auto lowerPage = fetchLedgerPage(cursor, lowerSequence, limit);
            std::vector<ripple::uint256> keys;
            std::transform(
                std::move_iterator(lowerPage.objects.begin()),
                std::move_iterator(lowerPage.objects.end()),
                std::back_inserter(keys),
                [](auto&& elt) { return std::move(elt.key); });
            auto objs = fetchLedgerObjects(keys, ledgerSequence);
            for (size_t i = 0; i < keys.size(); ++i)
            {
                auto& obj = objs[i];
                auto& key = keys[i];
                if (obj.size())
                    page.objects.push_back({std::move(key), std::move(obj)});
            }
            std::sort(
                page.objects.begin(), page.objects.end(), [](auto a, auto b) {
                    return a.key < b.key;
                });
            page.warning = "Data may be incomplete";
        }
        if (page.objects.size() >= limit)
        {
            page.objects.resize(limit);
            page.cursor = page.objects.back().key;
        }
        return page;
    }

    std::optional<LedgerObject>
    fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence)
    {
        auto page = fetchLedgerPage({++key}, ledgerSequence, 1);
        if (page.objects.size())
            return page.objects[0];
        return {};
    }
    virtual LedgerPage
    doFetchLedgerPage(
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
        indexer_.addKey(std::move(key256));
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
