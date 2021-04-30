#include <reporting/BackendInterface.h>

namespace Backend {
BackendIndexer::BackendIndexer(boost::json::object const& config)
    : shift_(config.at("indexer_shift").as_int64())
{
    work_.emplace(ioc_);
    ioThread_ = std::thread{[this]() { ioc_.run(); }};
};
BackendIndexer::~BackendIndexer()
{
    std::unique_lock lck(mutex_);
    work_.reset();
    ioThread_.join();
}

void
BackendIndexer::addKey(ripple::uint256 const& key)
{
    std::unique_lock lck(mtx);
    keys.insert(key);
    keysCumulative.insert(key);
}
void
BackendIndexer::addKeyAsync(ripple::uint256 const& key)
{
    std::unique_lock lck(mtx);
    keysCumulative.insert(key);
}
void
BackendIndexer::deleteKey(ripple::uint256 const& key)
{
    std::unique_lock lck(mtx);
    keysCumulative.erase(key);
    if (populatingCacheAsync)
        deletedKeys.insert(key);
}

void
BackendIndexer::addBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    std::unique_lock lck(mtx);
    books[book].insert(offerKey);
    booksCumulative[book].insert(offerKey);
}
void
BackendIndexer::addBookOfferAsync(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    std::unique_lock lck(mtx);
    booksCumulative[book].insert(offerKey);
}
void
BackendIndexer::deleteBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    std::unique_lock lck(mtx);
    booksCumulative[book].erase(offerKey);
    if (populatingCacheAsync)
        deletedBooks[book].insert(offerKey);
}

void
BackendIndexer::clearCaches()
{
    keysCumulative = {};
    booksCumulative = {};
}

void
BackendIndexer::populateCaches(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    if (!sequence)
        sequence = backend.fetchLatestLedgerSequence();
    if (!sequence)
        return;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sequence = " << std::to_string(*sequence);
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage(cursor, *sequence, 2048);
            if (warning)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " performing index repair";
                uint32_t lower = (*sequence - 1) >> shift_ << shift_;
                populateCaches(backend, lower);
                writeNext(lower, backend);
                clearCaches();
                continue;
            }
            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
            for (auto& obj : objects)
            {
                addKeyAsync(obj.key);
                if (isOffer(obj.blob))
                {
                    auto book = getBook(obj.blob);
                    addBookOfferAsync(book, obj.key);
                }
            }
            if (!cursor)
                break;
        }
        catch (DatabaseTimeout const& e)
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " Database timeout fetching keys";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    // Do reconcilation. Remove anything from keys or books that shouldn't be
    // there
    {
        std::unique_lock lck(mtx);
        populatingCacheAsync = false;
    }
    auto tip = backend.fetchLatestLedgerSequence();
    for (auto& key : deletedKeys)
    {
        deleteKey(key);
    }
    for (auto& book : deletedBooks)
    {
        for (auto& offer : book.second)
        {
            deleteBookOffer(book.first, offer);
        }
    }
    {
        std::unique_lock lck(mtx);
        deletedKeys = {};
        deletedBooks = {};
        cv_.notify_one();
    }
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. keys.size() = " << std::to_string(keysCumulative.size());
}
void
BackendIndexer::populateCachesAsync(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    if (keysCumulative.size() > 0)
    {
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " caches already populated. returning";
        return;
    }
    {
        std::unique_lock lck(mtx);
        populatingCacheAsync = true;
    }
    BOOST_LOG_TRIVIAL(info) << __func__;
    boost::asio::post(ioc_, [this, sequence, &backend]() {
        populateCaches(backend, sequence);
    });
}

void
BackendIndexer::waitForCaches()
{
    std::unique_lock lck(mtx);
    cv_.wait(lck, [this]() {
        return !populatingCacheAsync && deletedKeys.size() == 0;
    });
}

void
BackendIndexer::writeNext(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);
    bool isFlag = (ledgerSequence % (1 << shift_)) == 0;
    if (!backend.fetchLedgerRange())
    {
        isFlag = true;
    }

    if (isFlag)
    {
        waitForCaches();
        auto booksCopy = booksCumulative;
        auto keysCopy = keysCumulative;
        boost::asio::post(ioc_, [=, &backend]() {
            uint32_t nextSeq =
                ((ledgerSequence >> shift_ << shift_) + (1 << shift_));
            ripple::uint256 zero = {};
            BOOST_LOG_TRIVIAL(info) << __func__ << " booksCumulative.size() = "
                                    << std::to_string(booksCumulative.size());
            backend.writeBooks(booksCopy, nextSeq);
            backend.writeBooks({{zero, {zero}}}, nextSeq);
            BOOST_LOG_TRIVIAL(info) << __func__ << " wrote books";
            BOOST_LOG_TRIVIAL(info) << __func__ << " keysCumulative.size() = "
                                    << std::to_string(keysCumulative.size());
            backend.writeKeys(keysCopy, nextSeq);
            backend.writeKeys({zero}, nextSeq);
            BOOST_LOG_TRIVIAL(info) << __func__ << " wrote keys";
        });
    }
}

void
BackendIndexer::finish(uint32_t ledgerSequence, BackendInterface const& backend)
{
    bool isFlag = ledgerSequence % (1 << shift_) == 0;
    if (!backend.fetchLedgerRange())
    {
        isFlag = true;
    }
    uint32_t nextSeq = ((ledgerSequence >> shift_ << shift_) + (1 << shift_));
    uint32_t curSeq = isFlag ? ledgerSequence : nextSeq;
    backend.writeKeys(keys, curSeq);
    keys = {};
    backend.writeBooks(books, curSeq);
    books = {};

}  // namespace Backend
}  // namespace Backend
