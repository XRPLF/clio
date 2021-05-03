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
    keys.insert(key);
    keysCumulative.insert(key);
}
void
BackendIndexer::deleteKey(ripple::uint256 const& key)
{
    keysCumulative.erase(key);
}

void
BackendIndexer::addBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    books[book].insert(offerKey);
    booksCumulative[book].insert(offerKey);
}
void
BackendIndexer::deleteBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    booksCumulative[book].erase(offerKey);
}

void
BackendIndexer::clearCaches()
{
    keysCumulative = {};
    booksCumulative = {};
}
void
BackendIndexer::populateCaches(BackendInterface const& backend)
{
    if (keysCumulative.size() > 0)
    {
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " caches already populated. returning";
        return;
    }
    auto tip = backend.fetchLatestLedgerSequence();
    if (!tip)
        return;
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor] =
                backend.fetchLedgerPage(cursor, *tip, 2048);
            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
            for (auto& obj : objects)
            {
                keysCumulative.insert(obj.key);
                if (isOffer(obj.blob))
                {
                    auto book = getBook(obj.blob);
                    booksCumulative[book].insert(obj.key);
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
        uint32_t nextSeq =
            ((ledgerSequence >> shift_ << shift_) + (1 << shift_));
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " actually doing the write. keysCumulative.size() = "
            << std::to_string(keysCumulative.size());
        backend.writeKeys(keysCumulative, nextSeq);
        BOOST_LOG_TRIVIAL(info) << __func__ << " wrote keys";

        backend.writeBooks(booksCumulative, nextSeq);
        BOOST_LOG_TRIVIAL(info) << __func__ << " wrote books";
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
