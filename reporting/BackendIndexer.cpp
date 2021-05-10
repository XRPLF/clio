#include <reporting/BackendInterface.h>

namespace Backend {
BackendIndexer::BackendIndexer(boost::json::object const& config)
{
    if (config.contains("indexer_key_shift"))
        keyShift_ = config.at("indexer_key_shift").as_int64();
    if (config.contains("indexer_book_shift"))
        bookShift_ = config.at("indexer_book_shift").as_int64();
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
writeKeyFlagLedger(
    uint32_t ledgerSequence,
    uint32_t shift,
    BackendInterface const& backend,
    std::unordered_set<ripple::uint256> const& keys)
{
    uint32_t nextFlag = ((ledgerSequence >> shift << shift) + (1 << shift));
    ripple::uint256 zero = {};
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. ledgerSequence = " << std::to_string(ledgerSequence)
        << " nextFlag = " << std::to_string(nextFlag)
        << " keys.size() = " << std::to_string(keys.size());
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage({}, nextFlag, 1);
            if (!warning)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " flag ledger already written. sequence = "
                    << std::to_string(ledgerSequence)
                    << " next flag = " << std::to_string(nextFlag)
                    << "returning";
                return;
            }
            break;
        }
        catch (DatabaseTimeout& t)
        {
            ;
        }
    }
    auto start = std::chrono::system_clock::now();

    backend.writeKeys(keys, nextFlag, true);
    backend.writeKeys({zero}, nextFlag, true);
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. ledgerSequence = " << std::to_string(ledgerSequence)
        << " nextFlag = " << std::to_string(nextFlag)
        << " keys.size() = " << std::to_string(keys.size())
        << std::chrono::duration_cast<std::chrono::seconds>(end - start)
               .count();
}
void
writeBookFlagLedger(
    uint32_t ledgerSequence,
    uint32_t shift,
    BackendInterface const& backend,
    std::unordered_map<
        ripple::uint256,
        std::unordered_set<ripple::uint256>> const& books)

{
    uint32_t nextFlag = ((ledgerSequence >> shift << shift) + (1 << shift));
    ripple::uint256 zero = {};
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. ledgerSequence = " << std::to_string(ledgerSequence)
        << " nextFlag = " << std::to_string(nextFlag)
        << " books.size() = " << std::to_string(books.size());
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchBookOffers(zero, nextFlag, 1);
            if (!warning)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " flag ledger already written. sequence = "
                    << std::to_string(ledgerSequence)
                    << " next flag = " << std::to_string(nextFlag)
                    << "returning";
                return;
            }
            break;
        }
        catch (DatabaseTimeout& t)
        {
            ;
        }
    }
    auto start = std::chrono::system_clock::now();
    backend.writeBooks(books, nextFlag, true);
    backend.writeBooks({{zero, {zero}}}, nextFlag, true);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. ledgerSequence = " << std::to_string(ledgerSequence)
        << " nextFlag = " << std::to_string(nextFlag)
        << " books.size() = " << std::to_string(books.size()) << " time = "
        << std::chrono::duration_cast<std::chrono::seconds>(end - start)
               .count();
}

void
BackendIndexer::clearCaches()
{
    keysCumulative = {};
    booksCumulative = {};
}

void
BackendIndexer::doBooksRepair(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    if (!sequence)
    {
        auto rng = backend.fetchLedgerRangeNoThrow();
        if (!rng)
            return;
        sequence = rng->maxSequence;
    }
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sequence = " << std::to_string(*sequence);
    ripple::uint256 zero = {};
    while (true)
    {
        try
        {
            auto [objects, cursor, warning] =
                backend.fetchBookOffers(zero, *sequence, 1);
            if (!warning)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " flag ledger already written. sequence = "
                    << std::to_string(*sequence) << "returning";
                return;
            }
            else
            {
                uint32_t lower = (*sequence - 1) >> bookShift_ << bookShift_;
                doBooksRepair(backend, lower);
            }
            break;
        }
        catch (DatabaseTimeout& t)
        {
            ;
        }
    }
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage(cursor, *sequence, 2048);

            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
            for (auto& obj : objects)
            {
                if (isOffer(obj.blob))
                {
                    auto book = getBook(obj.blob);
                    booksRepair[book].insert(obj.key);
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
    writeBookFlagLedger(*sequence, bookShift_, backend, booksRepair);
    booksRepair = {};
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " finished. sequence = " << std::to_string(*sequence);
}
void
BackendIndexer::doKeysRepair(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    if (!sequence)
    {
        auto rng = backend.fetchLedgerRangeNoThrow();
        if (!rng)
            return;
        sequence = rng->maxSequence;
    }
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sequence = " << std::to_string(*sequence);
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage(cursor, *sequence, 2048);
            // no cursor means this is the first page
            if (!cursor)
            {
                // if there is no warning, we don't need to do a repair
                // warning only shows up on the first page
                if (!warning)
                {
                    BOOST_LOG_TRIVIAL(info)
                        << __func__
                        << " flag ledger already written. returning";
                    return;
                }
                else
                {
                    uint32_t lower = (*sequence - 1) >> keyShift_ << keyShift_;
                    doKeysRepair(backend, lower);
                }
            }

            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
            for (auto& obj : objects)
            {
                keysRepair.insert(obj.key);
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
    writeKeyFlagLedger(*sequence, keyShift_, backend, keysRepair);
    keysRepair = {};
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " finished. sequence = " << std::to_string(*sequence);
}

void
BackendIndexer::populateCaches(BackendInterface const& backend)
{
    auto rng = backend.fetchLedgerRangeNoThrow();
    if (!rng)
        return;
    uint32_t sequence = rng->maxSequence;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sequence = " << std::to_string(sequence);
    doBooksRepair(backend, sequence);
    doKeysRepair(backend, sequence);
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage(cursor, sequence, 2048);
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
    // Do reconcilation. Remove anything from keys or books that shouldn't
    // be there
    {
        std::unique_lock lck(mtx);
        populatingCacheAsync = false;
    }
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
BackendIndexer::populateCachesAsync(BackendInterface const& backend)
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
    boost::asio::post(ioc_, [this, &backend]() { populateCaches(backend); });
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
BackendIndexer::writeKeyFlagLedgerAsync(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);

    waitForCaches();
    auto keysCopy = keysCumulative;
    boost::asio::post(ioc_, [=, this, &backend]() {
        writeKeyFlagLedger(ledgerSequence, keyShift_, backend, keysCopy);
    });
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);
}
void
BackendIndexer::writeBookFlagLedgerAsync(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);

    waitForCaches();
    auto booksCopy = booksCumulative;
    boost::asio::post(ioc_, [=, this, &backend]() {
        writeBookFlagLedger(ledgerSequence, bookShift_, backend, booksCopy);
    });
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);
}

void
BackendIndexer::finish(uint32_t ledgerSequence, BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);
    bool isFirst = false;
    uint32_t keyIndex = getKeyIndexOfSeq(ledgerSequence);
    uint32_t bookIndex = getKeyIndexOfSeq(ledgerSequence);
    auto rng = backend.fetchLedgerRangeNoThrow();
    if (!rng || rng->minSequence == ledgerSequence)
    {
        isFirst = true;
        keyIndex = bookIndex = ledgerSequence;
    }
    backend.writeKeys(keys, keyIndex);
    backend.writeBooks(books, bookIndex);
    if (isFirst)
    {
        ripple::uint256 zero = {};
        backend.writeBooks({{zero, {zero}}}, ledgerSequence);
        backend.writeKeys({zero}, ledgerSequence);
        writeBookFlagLedgerAsync(ledgerSequence, backend);
        writeKeyFlagLedgerAsync(ledgerSequence, backend);
    }
    keys = {};
    books = {};
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);

}  // namespace Backend
}  // namespace Backend
