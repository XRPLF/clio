#include <functional>
#include <reporting/CassandraBackend.h>
#include <reporting/DBHelpers.h>
#include <unordered_map>
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
template <class T, class F>
void
processAsyncWriteResponse(T& requestParams, CassFuture* fut, F func)
{
    CassandraBackend const& backend = *requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.getIOContext(),
                std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &func](
                              const boost::system::error_code& error) {
            func(requestParams, true);
        });
    }
    else
    {
        backend.finishAsyncWrite();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}
// Process the result of an asynchronous write. Retry on error
// @param fut cassandra future associated with the write
// @param cbData struct that holds the request parameters
void
flatMapWriteCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->write(params, retry);
    };

    processAsyncWriteResponse(requestParams, fut, func);
}

void
flatMapWriteBookCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeBook(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
/*

void
retryWriteKey(CassandraBackend::WriteCallbackData& requestParams, bool isRetry)
{
    auto const& backend = *requestParams.backend;
    if (requestParams.isDeleted)
        backend.writeDeletedKey(requestParams, true);
    else
        backend.writeKey(requestParams, true);
}

void
flatMapWriteKeyCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    processAsyncWriteResponse(requestParams, fut, retryWriteKey);
}

void
flatMapGetCreatedCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    CassandraBackend const& backend = *requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
        BOOST_LOG_TRIVIAL(info) << __func__;
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeKey(requestParams, true);
        });
    }
    else
    {
        auto finish = [&backend]() {
            --(backend.numRequestsOutstanding_);

            backend.throttleCv_.notify_all();
            if (backend.numRequestsOutstanding_ == 0)
                backend.syncCv_.notify_all();
        };
        CassandraResult result{cass_future_get_result(fut)};

        if (!result)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch get row error : " << rc
                                     << ", " << cass_error_desc(rc);
            finish();
            return;
        }
        requestParams.createdSequence = result.getUInt32();
        backend.writeDeletedKey(requestParams, false);
    }
}
*/
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteTransactionCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteTransactionCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeTransaction(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
void
flatMapWriteAccountTxCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteAccountTxCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteAccountTxCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeAccountTx(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
void
flatMapWriteLedgerHeaderCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHeaderCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHeaderCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeLedgerHeader(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}

void
flatMapWriteLedgerHashCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHashCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHashCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeLedgerHash(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::ReadCallbackData& requestParams =
        *static_cast<CassandraBackend::ReadCallbackData*>(cbData);
    auto func = [](auto& params) { params.backend.read(params); };
    CassandraAsyncResult asyncResult{requestParams, fut, func};
    if (asyncResult.timedOut())
        requestParams.result.transaction = {0};
    CassandraResult& result = asyncResult.getResult();

    if (!!result)
    {
        requestParams.result = {
            result.getBytes(), result.getBytes(), result.getUInt32()};
    }
}

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadObjectCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::ReadObjectCallbackData& requestParams =
        *static_cast<CassandraBackend::ReadObjectCallbackData*>(cbData);
    auto func = [](auto& params) { params.backend.readObject(params); };
    CassandraAsyncResult asyncResult{requestParams, fut, func};
    if (asyncResult.timedOut())
        requestParams.result = {0};
    CassandraResult& result = asyncResult.getResult();

    if (!!result)
    {
        requestParams.result = result.getBytes();
    }
}

std::optional<LedgerRange>
CassandraBackend::fetchLedgerRange() const
{
    BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
    CassandraStatement statement{selectLedgerRange_};
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    LedgerRange range;
    range.maxSequence = range.minSequence = result.getUInt32();
    if (result.nextRow())
    {
        range.maxSequence = result.getUInt32();
    }
    if (range.minSequence > range.maxSequence)
    {
        std::swap(range.minSequence, range.maxSequence);
    }
    return range;
}
std::vector<TransactionAndMetadata>
CassandraBackend::fetchAllTransactionsInLedger(uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectAllTransactionsInLedger_};
    statement.bindInt(ledgerSequence);
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    std::vector<TransactionAndMetadata> txns;
    do
    {
        txns.push_back(
            {result.getBytes(), result.getBytes(), result.getUInt32()});
    } while (result.nextRow());
    return txns;
}
std::vector<ripple::uint256>
CassandraBackend::fetchAllTransactionHashesInLedger(
    uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectAllTransactionHashesInLedger_};
    statement.bindInt(ledgerSequence);
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    std::vector<ripple::uint256> hashes;
    do
    {
        hashes.push_back(result.getUInt256());
    } while (result.nextRow());
    return hashes;
}

LedgerPage
CassandraBackend::fetchLedgerPage2(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    BOOST_LOG_TRIVIAL(trace) << __func__;
    std::optional<ripple::uint256> currentCursor = cursor;
    std::vector<LedgerObject> objects;
    uint32_t curLimit = limit;
    while (objects.size() < limit)
    {
        CassandraStatement statement{selectLedgerPage_};

        int64_t intCursor = INT64_MIN;
        if (currentCursor)
        {
            auto token = getToken(currentCursor->data());
            if (token)
                intCursor = *token;
        }
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " - cursor = " << std::to_string(intCursor)
            << " , sequence = " << std::to_string(ledgerSequence)
            << ", - limit = " << std::to_string(limit);
        statement.bindInt(intCursor);
        statement.bindInt(ledgerSequence);
        statement.bindUInt(curLimit);

        CassandraResult result = executeSyncRead(statement);

        if (!!result)
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " - got keys - size = " << result.numRows();

            size_t prevSize = objects.size();
            do
            {
                std::vector<unsigned char> object = result.getBytes();
                if (object.size())
                {
                    objects.push_back({result.getUInt256(), std::move(object)});
                }
            } while (result.nextRow());
            size_t prevBatchSize = objects.size() - prevSize;
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " - added to objects. size = " << objects.size();
            if (result.numRows() < curLimit)
            {
                currentCursor = {};
                break;
            }
            if (objects.size() < limit)
            {
                curLimit = 2048;
            }
            assert(objects.size());
            currentCursor = objects[objects.size() - 1].key;
        }
    }
    if (objects.size())
        return {objects, currentCursor};

    return {{}, {}};
}

std::vector<LedgerObject>
CassandraBackend::fetchLedgerDiff(uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectLedgerDiff_};
    statement.bindInt(ledgerSequence);

    auto start = std::chrono::system_clock::now();
    CassandraResult result = executeSyncRead(statement);

    auto mid = std::chrono::system_clock::now();
    if (!result)
        return {};
    std::vector<LedgerObject> objects;
    do
    {
        objects.push_back({result.getUInt256(), result.getBytes()});
    } while (result.nextRow());
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " Fetched diff. Fetch time = "
        << std::to_string((mid - start).count() / 1000000000.0)
        << " . total time = "
        << std::to_string((end - start).count() / 1000000000.0);
    return objects;
}
LedgerPage
CassandraBackend::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    LedgerPage page;
    bool cursorIsInt = false;
    if (cursor && !cursor->isZero())
    {
        bool foundNonZero = false;
        for (size_t i = 0; i < 28 && !foundNonZero; ++i)
        {
            if (cursor->data()[i] != 0)
                foundNonZero = true;
        }
        cursorIsInt = !foundNonZero;
    }
    if (cursor)
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " - Cursor = " << ripple::strHex(*cursor)
            << " : cursorIsInt = " << std::to_string(cursorIsInt);
    if (!cursor || !cursorIsInt)
    {
        BOOST_LOG_TRIVIAL(info) << __func__ << " Using base ledger";
        CassandraStatement statement{selectKeys_};
        uint32_t upper = (ledgerSequence >> indexerShift_) << indexerShift_;
        if (upper != ledgerSequence)
            upper += (1 << indexerShift_);
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " upper is " << std::to_string(upper);
        statement.bindInt(upper);
        if (cursor)
            statement.bindBytes(*cursor);
        else
        {
            ripple::uint256 zero;
            statement.bindBytes(zero);
        }
        statement.bindUInt(limit);
        CassandraResult result = executeSyncRead(statement);
        BOOST_LOG_TRIVIAL(info) << __func__ << " Using base ledger. Got keys";
        if (!!result)
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " - got keys - size = " << result.numRows();
            std::vector<ripple::uint256> keys;

            do
            {
                keys.push_back(result.getUInt256());
            } while (result.nextRow());
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Using base ledger. Read keys";
            auto objects = fetchLedgerObjects(keys, ledgerSequence);
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Using base ledger. Got objects";
            if (objects.size() != keys.size())
                throw std::runtime_error(
                    "Mismatch in size of objects and keys");
            if (keys.size() == limit)
                page.cursor = keys[keys.size() - 1];
            else if (ledgerSequence < upper)
                page.cursor = upper - 1;

            if (cursor)
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " Cursor = " << ripple::strHex(*page.cursor);

            for (size_t i = 0; i < objects.size(); ++i)
            {
                auto& obj = objects[i];
                auto& key = keys[i];
                if (obj.size())
                {
                    page.objects.push_back({std::move(key), std::move(obj)});
                }
            }
            return page;
        }
    }
    else
    {
        uint32_t curSequence = 0;
        for (size_t i = 28; i < 32; ++i)
        {
            uint32_t digit = cursor->data()[i];
            digit = digit << (8 * (31 - i));
            curSequence += digit;
        }
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " Using ledger diffs. Sequence = " << curSequence
            << " size_of uint32_t " << std::to_string(sizeof(uint32_t))
            << " cursor = " << ripple::strHex(*cursor);
        auto diff = fetchLedgerDiff(curSequence);
        BOOST_LOG_TRIVIAL(info) << __func__ << " diff size = " << diff.size();
        std::vector<ripple::uint256> deletedKeys;
        for (auto& obj : diff)
        {
            if (obj.blob.size() == 0)
                deletedKeys.push_back(std::move(obj.key));
        }
        auto objects = fetchLedgerObjects(deletedKeys, ledgerSequence);
        if (objects.size() != deletedKeys.size())
            throw std::runtime_error("Mismatch in size of objects and keys");
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " deleted keys size = " << deletedKeys.size();
        for (size_t i = 0; i < objects.size(); ++i)
        {
            auto& obj = objects[i];
            auto& key = deletedKeys[i];
            if (obj.size())
            {
                page.objects.push_back({std::move(key), std::move(obj)});
            }
        }
        if (curSequence - 1 >= ledgerSequence)
            page.cursor = curSequence - 1;
        return page;
        // do the diff algorithm
    }
    return {{}, {}};
}
std::vector<Blob>
CassandraBackend::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    uint32_t sequence) const
{
    std::size_t const numKeys = keys.size();
    BOOST_LOG_TRIVIAL(trace)
        << "Fetching " << numKeys << " records from Cassandra";
    std::atomic_uint32_t numFinished = 0;
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<Blob> results{numKeys};
    std::vector<std::shared_ptr<ReadObjectCallbackData>> cbs;
    cbs.reserve(numKeys);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        cbs.push_back(std::make_shared<ReadObjectCallbackData>(
            *this, keys[i], sequence, results[i], cv, numFinished, numKeys));
        readObject(*cbs[i]);
    }
    assert(results.size() == cbs.size());

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numFinished, &numKeys]() { return numFinished == numKeys; });
    for (auto const& res : results)
    {
        if (res.size() == 1 && res[0] == 0)
            throw DatabaseTimeout();
    }

    BOOST_LOG_TRIVIAL(trace)
        << "Fetched " << numKeys << " records from Cassandra";
    return results;
}
std::pair<std::vector<LedgerObject>, std::optional<ripple::uint256>>
CassandraBackend::fetchBookOffers(
    ripple::uint256 const& book,
    uint32_t sequence,
    std::uint32_t limit,
    std::optional<ripple::uint256> const& cursor) const
{
    CassandraStatement statement{selectBook_};
    statement.bindBytes(book);
    uint32_t upper = (sequence >> 8) << 8;
    if (upper != sequence)
        upper += (1 << 8);
    statement.bindInt(upper);
    if (cursor)
        statement.bindBytes(*cursor);
    else
    {
        ripple::uint256 zero = {};
        statement.bindBytes(zero);
    }
    statement.bindUInt(limit);
    CassandraResult result = executeSyncRead(statement);

    BOOST_LOG_TRIVIAL(debug) << __func__ << " - got keys";
    std::vector<ripple::uint256> keys;
    if (!result)
        return {{}, {}};
    do
    {
        keys.push_back(result.getUInt256());
    } while (result.nextRow());

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " - populated keys. num keys = " << keys.size();
    if (keys.size())
    {
        std::vector<LedgerObject> results;
        std::vector<Blob> objs = fetchLedgerObjects(keys, sequence);
        for (size_t i = 0; i < objs.size(); ++i)
        {
            if (objs[i].size() != 0)
                results.push_back({keys[i], objs[i]});
        }
        return {results, results[results.size() - 1].key};
    }

    return {{}, {}};
}
struct WriteBookCallbackData
{
    CassandraBackend const& backend;
    ripple::uint256 book;
    ripple::uint256 offerKey;
    uint32_t ledgerSequence;
    std::condition_variable& cv;
    std::atomic_uint32_t& numRemaining;
    std::mutex& mtx;
    uint32_t currentRetries = 0;
    WriteBookCallbackData(
        CassandraBackend const& backend,
        ripple::uint256 const& book,
        ripple::uint256 const& offerKey,
        uint32_t ledgerSequence,
        std::condition_variable& cv,
        std::mutex& mtx,
        std::atomic_uint32_t& numRemaining)
        : backend(backend)
        , book(book)
        , offerKey(offerKey)
        , ledgerSequence(ledgerSequence)
        , cv(cv)
        , mtx(mtx)
        , numRemaining(numRemaining)

    {
    }
};
void
writeBookCallback(CassFuture* fut, void* cbData);
void
writeBook2(WriteBookCallbackData& cb)
{
    CassandraStatement statement{cb.backend.getInsertBookPreparedStatement()};
    statement.bindBytes(cb.book);
    statement.bindInt(cb.ledgerSequence);
    statement.bindBytes(cb.offerKey);
    // Passing isRetry as true bypasses incrementing numOutstanding
    cb.backend.executeAsyncWrite(statement, writeBookCallback, cb, true);
}
void
writeBookCallback(CassFuture* fut, void* cbData)
{
    WriteBookCallbackData& requestParams =
        *static_cast<WriteBookCallbackData*>(cbData);

    CassandraBackend const& backend = requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert key error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.getIOContext(),
                std::chrono::steady_clock::now() + wait);
        timer->async_wait(
            [timer, &requestParams](const boost::system::error_code& error) {
                writeBook2(requestParams);
            });
    }
    else
    {
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Finished a write request";
        {
            std::lock_guard lck(requestParams.mtx);
            --requestParams.numRemaining;
            requestParams.cv.notify_one();
        }
    }
}

struct WriteKeyCallbackData
{
    CassandraBackend const& backend;
    ripple::uint256 key;
    uint32_t ledgerSequence;
    std::condition_variable& cv;
    std::atomic_uint32_t& numRemaining;
    std::mutex& mtx;
    uint32_t currentRetries = 0;
    WriteKeyCallbackData(
        CassandraBackend const& backend,
        ripple::uint256 const& key,
        uint32_t ledgerSequence,
        std::condition_variable& cv,
        std::mutex& mtx,
        std::atomic_uint32_t& numRemaining)
        : backend(backend)
        , key(key)
        , ledgerSequence(ledgerSequence)
        , cv(cv)
        , mtx(mtx)
        , numRemaining(numRemaining)

    {
    }
};
void
writeKeyCallback(CassFuture* fut, void* cbData);
void
writeKey(WriteKeyCallbackData& cb)
{
    CassandraStatement statement{cb.backend.getInsertKeyPreparedStatement()};
    statement.bindInt(cb.ledgerSequence);
    statement.bindBytes(cb.key);
    // Passing isRetry as true bypasses incrementing numOutstanding
    cb.backend.executeAsyncWrite(statement, writeKeyCallback, cb, true);
}
void
writeKeyCallback(CassFuture* fut, void* cbData)
{
    WriteKeyCallbackData& requestParams =
        *static_cast<WriteKeyCallbackData*>(cbData);

    CassandraBackend const& backend = requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert key error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.getIOContext(),
                std::chrono::steady_clock::now() + wait);
        timer->async_wait(
            [timer, &requestParams](const boost::system::error_code& error) {
                writeKey(requestParams);
            });
    }
    else
    {
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Finished a write request";
        {
            std::lock_guard lck(requestParams.mtx);
            --requestParams.numRemaining;
            requestParams.cv.notify_one();
        }
    }
}

bool
CassandraBackend::writeKeys(
    std::unordered_set<ripple::uint256>& keys,
    uint32_t ledgerSequence) const
{
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " Ledger = " << std::to_string(ledgerSequence)
        << " . num keys = " << std::to_string(keys.size());
    std::atomic_uint32_t numRemaining = keys.size();
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<std::shared_ptr<WriteKeyCallbackData>> cbs;
    cbs.reserve(keys.size());
    uint32_t concurrentLimit = maxRequestsOutstanding / 2;
    uint32_t numSubmitted = 0;
    for (auto& key : keys)
    {
        cbs.push_back(std::make_shared<WriteKeyCallbackData>(
            *this, key, ledgerSequence, cv, mtx, numRemaining));
        writeKey(*cbs.back());
        ++numSubmitted;
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Submitted a write request";
        std::unique_lock<std::mutex> lck(mtx);
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Got the mutex";
        cv.wait(lck, [&numRemaining, numSubmitted, concurrentLimit, &keys]() {
            BOOST_LOG_TRIVIAL(trace) << std::to_string(numSubmitted) << " "
                                     << std::to_string(numRemaining) << " "
                                     << std::to_string(keys.size()) << " "
                                     << std::to_string(concurrentLimit);
            // keys.size() - i is number submitted. keys.size() -
            // numRemaining is number completed Difference is num
            // outstanding
            return (numSubmitted - (keys.size() - numRemaining)) <
                concurrentLimit;
        });
        if (numSubmitted % 100000 == 0)
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Submitted " << std::to_string(numSubmitted)
                << " write requests. Completed "
                << (keys.size() - numRemaining);
    }

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
    return true;
}

bool
CassandraBackend::writeBooks(
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>&
        books,
    uint32_t ledgerSequence,
    uint32_t numOffers) const
{
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " Ledger = " << std::to_string(ledgerSequence)
        << " . num books = " << std::to_string(books.size());
    std::atomic_uint32_t numRemaining = numOffers;
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<std::shared_ptr<WriteBookCallbackData>> cbs;
    uint32_t concurrentLimit = maxRequestsOutstanding / 2;
    uint32_t numSubmitted = 0;
    auto start = std::chrono::system_clock::now();
    for (auto& book : books)
    {
        for (auto& offer : book.second)
        {
            cbs.push_back(std::make_shared<WriteBookCallbackData>(
                *this,
                book.first,
                offer,
                ledgerSequence,
                cv,
                mtx,
                numRemaining));
            writeBook2(*cbs.back());
            ++numSubmitted;
            BOOST_LOG_TRIVIAL(trace) << __func__ << "Submitted a write request";
            std::unique_lock<std::mutex> lck(mtx);
            BOOST_LOG_TRIVIAL(trace) << __func__ << "Got the mutex";
            cv.wait(
                lck,
                [&numRemaining, numSubmitted, concurrentLimit, numOffers]() {
                    BOOST_LOG_TRIVIAL(trace)
                        << std::to_string(numSubmitted) << " "
                        << std::to_string(numRemaining) << " "
                        << std::to_string(numOffers) << " "
                        << std::to_string(concurrentLimit);
                    return (numSubmitted - (numOffers - numRemaining)) <
                        concurrentLimit;
                });
            if (numSubmitted % 1000 == 0)
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " Submitted " << std::to_string(numSubmitted)
                    << " write requests. Completed "
                    << (numOffers - numRemaining);
        }
    }
    BOOST_LOG_TRIVIAL(info) << __func__
                            << "Submitted all book writes. Waiting for them to "
                               "finish. num submitted = "
                            << std::to_string(numSubmitted);
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
    BOOST_LOG_TRIVIAL(info) << __func__ << "Finished writing books";
    return true;
}

bool
CassandraBackend::runIndexer(uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectKeys_};
    statement.bindInt(ledgerSequence);
    ripple::uint256 zero;
    statement.bindBytes(zero);
    statement.bindUInt(1);
    CassandraResult result = executeSyncRead(statement);
    if (!!result)
    {
        BOOST_LOG_TRIVIAL(info) << "Ledger " << std::to_string(ledgerSequence)
                                << " already indexed. Returning";
        return false;
    }
    auto start = std::chrono::system_clock::now();
    constexpr uint32_t limit = 2048;
    std::unordered_set<ripple::uint256> keys;
    std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
        books;
    std::optional<ripple::uint256> cursor;
    size_t numOffers = 0;
    while (true)
    {
        try
        {
            auto [objects, curCursor] =
                fetchLedgerPage2(cursor, ledgerSequence, limit);
            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
            for (auto& obj : objects)
            {
                if (isOffer(obj.blob))
                {
                    auto bookDir = getBook(obj.blob);
                    books[bookDir].insert(obj.key);
                    ++numOffers;
                }
                keys.insert(std::move(obj.key));
                if (keys.size() % 100000 == 0)
                    BOOST_LOG_TRIVIAL(info)
                        << __func__ << " Fetched "
                        << std::to_string(keys.size()) << "keys";
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
    auto mid = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << "Fetched all keys from ledger "
        << std::to_string(ledgerSequence) << " . num keys = " << keys.size()
        << " num books = " << books.size() << " num offers = " << numOffers
        << " . Took " << (mid - start).count() / 1000000000.0;
    writeKeys(keys, ledgerSequence);
    writeBooks(books, ledgerSequence, numOffers);
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << "Wrote all keys from ledger "
        << std::to_string(ledgerSequence) << " . num keys = " << keys.size()
        << " . Took " << (end - mid).count() / 1000000000.0
        << ". Entire operation took " << (end - start).count() / 1000000000.0;

    uint32_t prevLedgerSequence = ledgerSequence;
    uint32_t prevBooksLedgerSequence = ledgerSequence;
    uint32_t nextLedgerSequence =
        ((prevLedgerSequence >> indexerShift_) << indexerShift_);
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " next base = " << std::to_string(nextLedgerSequence);
    nextLedgerSequence += (1 << indexerShift_);
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " next = " << std::to_string(nextLedgerSequence);
    if (nextLedgerSequence == prevLedgerSequence)
    {
        nextLedgerSequence += (1 << indexerShift_);
    }
    while (true)
    {
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " Processing diffs. nextLedger = "
            << std::to_string(nextLedgerSequence);
        auto rng = fetchLedgerRange();
        if (rng->maxSequence < nextLedgerSequence)
            break;
        std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
            nextBooks;
        size_t nextOffers = 0;
        start = std::chrono::system_clock::now();
        for (size_t i = prevLedgerSequence + 1; i <= nextLedgerSequence; ++i)
        {
            // Get the diff and update keys
            auto objs = fetchLedgerDiff(i);
            std::unordered_set<ripple::uint256> deleted;
            for (auto const& obj : objs)
            {
                // remove deleted keys
                if (obj.blob.size() == 0)
                {
                    keys.erase(obj.key);
                    deleted.insert(obj.key);
                }
                else
                {
                    // insert other keys. keys is a set, so this is a noop if
                    // obj.key is already in keys
                    keys.insert(obj.key);
                    // if the object is an offer, add to nextBooks
                    if (isOffer(obj.blob))
                    {
                        auto book = getBook(obj.blob);
                        if (nextBooks[book].insert(obj.key).second)
                            ++nextOffers;
                    }
                }
            }
            // For any deleted keys, check if they are offer objects
            std::vector<ripple::uint256> deletedKeys{
                deleted.begin(), deleted.end()};
            auto deletedObjs = fetchLedgerObjects(deletedKeys, i - 1);
            for (size_t j = 0; j < deletedObjs.size(); ++j)
            {
                auto& obj = deletedObjs[j];
                auto& key = deletedKeys[j];
                if (!obj.size())
                {
                    BOOST_LOG_TRIVIAL(error)
                        << __func__
                        << " Deleted object is deleted in prior ledger. "
                        << ripple::strHex(key) << " " << std::to_string(i - 1);
                    throw std::runtime_error("Empty object");
                }
                // For any deleted keys, check if they are offer objects
                // Add key to nextBooks if is offer
                if (isOffer(obj))
                {
                    auto book = getBook(obj);
                    if (nextBooks[book].insert(key).second)
                        ++nextOffers;
                }
            }
            // books are written every 256 ledgers
            if (i % 256 == 0)
            {
                // Iterate through books from previous flag ledger, copying over
                // any that still exist
                for (auto& book : books)
                {
                    std::vector<ripple::uint256> offerKeys;
                    for (auto& offerKey : book.second)
                    {
                        offerKeys.push_back(offerKey);
                    }

                    auto offers =
                        fetchLedgerObjects(offerKeys, prevBooksLedgerSequence);
                    for (size_t i = 0; i < offerKeys.size(); ++i)
                    {
                        auto& offer = offers[i];
                        // if the offer was deleted prior to prevLedgerSequence,
                        // don't copy
                        if (offer.size() != 0)
                        {
                            auto book = getBook(offer);
                            if (nextBooks[book].insert(offerKeys[i]).second)
                                ++nextOffers;
                        }
                        else
                        {
                            BOOST_LOG_TRIVIAL(debug)
                                << __func__ << " skipping deleted offer";
                        }
                    }
                }
                writeBooks(nextBooks, i, nextOffers);
                prevBooksLedgerSequence = i;
                books = std::move(nextBooks);
                nextBooks = {};
                nextOffers = 0;
            }
        }
        end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(info)
            << __func__ << "Fetched all from diffs "
            << std::to_string(nextLedgerSequence)
            << " shift width = " << std::to_string(indexerShift_)
            << ". num keys = " << keys.size() << " . Took "
            << (end - start).count() / 1000000000.0
            << " prev ledger = " << std::to_string(prevLedgerSequence);
        writeKeys(keys, nextLedgerSequence);
        prevLedgerSequence = nextLedgerSequence;
        nextLedgerSequence = prevLedgerSequence + (1 << indexerShift_);
    }
    return true;
}

bool
CassandraBackend::doOnlineDelete(uint32_t minLedgerToKeep) const
{
    throw std::runtime_error("doOnlineDelete : unimplemented");
    return false;
}

void
CassandraBackend::open()
{
    std::cout << config_ << std::endl;
    auto getString = [this](std::string const& field) -> std::string {
        if (config_.contains(field))
        {
            auto jsonStr = config_[field].as_string();
            return {jsonStr.c_str(), jsonStr.size()};
        }
        return {""};
    };
    if (open_)
    {
        assert(false);
        BOOST_LOG_TRIVIAL(error) << "database is already open";
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    CassCluster* cluster = cass_cluster_new();
    if (!cluster)
        throw std::runtime_error("nodestore:: Failed to create CassCluster");

    std::string secureConnectBundle = getString("secure_connect_bundle");

    if (!secureConnectBundle.empty())
    {
        /* Setup driver to connect to the cloud using the secure connection
         * bundle */
        if (cass_cluster_set_cloud_secure_connection_bundle(
                cluster, secureConnectBundle.c_str()) != CASS_OK)
        {
            BOOST_LOG_TRIVIAL(error) << "Unable to configure cloud using the "
                                        "secure connection bundle: "
                                     << secureConnectBundle;
            throw std::runtime_error(
                "nodestore: Failed to connect using secure connection "
                "bundle");
            return;
        }
    }
    else
    {
        std::string contact_points = getString("contact_points");
        if (contact_points.empty())
        {
            throw std::runtime_error(
                "nodestore: Missing contact_points in Cassandra config");
        }
        CassError rc =
            cass_cluster_set_contact_points(cluster, contact_points.c_str());
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra contact_points: "
               << contact_points << ", result: " << rc << ", "
               << cass_error_desc(rc);

            throw std::runtime_error(ss.str());
        }

        int port = config_.contains("port") ? config_["port"].as_int64() : 0;
        if (port)
        {
            rc = cass_cluster_set_port(cluster, port);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra port: " << port
                   << ", result: " << rc << ", " << cass_error_desc(rc);

                throw std::runtime_error(ss.str());
            }
        }
    }
    cass_cluster_set_token_aware_routing(cluster, cass_true);
    CassError rc =
        cass_cluster_set_protocol_version(cluster, CASS_PROTOCOL_VERSION_V4);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting cassandra protocol version: "
           << ", result: " << rc << ", " << cass_error_desc(rc);

        throw std::runtime_error(ss.str());
    }

    std::string username = getString("username");
    if (username.size())
    {
        BOOST_LOG_TRIVIAL(debug)
            << "user = " << username.c_str()
            << " password = " << getString("password").c_str();
        cass_cluster_set_credentials(
            cluster, username.c_str(), getString("password").c_str());
    }

    unsigned int const workers = std::thread::hardware_concurrency();
    rc = cass_cluster_set_num_threads_io(cluster, workers);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra io threads to " << workers
           << ", result: " << rc << ", " << cass_error_desc(rc);
        throw std::runtime_error(ss.str());
    }

    cass_cluster_set_request_timeout(cluster, 10000);

    rc = cass_cluster_set_queue_size_io(
        cluster,
        maxRequestsOutstanding);  // This number needs to scale w/ the
                                  // number of request per sec
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra max core connections per "
              "host"
           << ", result: " << rc << ", " << cass_error_desc(rc);
        BOOST_LOG_TRIVIAL(error) << ss.str();
        throw std::runtime_error(ss.str());
    }

    std::string certfile = getString("certfile");
    if (certfile.size())
    {
        std::ifstream fileStream(
            boost::filesystem::path(certfile).string(), std::ios::in);
        if (!fileStream)
        {
            std::stringstream ss;
            ss << "opening config file " << certfile;
            throw std::system_error(errno, std::generic_category(), ss.str());
        }
        std::string cert(
            std::istreambuf_iterator<char>{fileStream},
            std::istreambuf_iterator<char>{});
        if (fileStream.bad())
        {
            std::stringstream ss;
            ss << "reading config file " << certfile;
            throw std::system_error(errno, std::generic_category(), ss.str());
        }

        CassSsl* context = cass_ssl_new();
        cass_ssl_set_verify_flags(context, CASS_SSL_VERIFY_NONE);
        rc = cass_ssl_add_trusted_cert(context, cert.c_str());
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra ssl context: " << rc
               << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }

        cass_cluster_set_ssl(cluster, context);
        cass_ssl_free(context);
    }

    std::string keyspace = getString("keyspace");
    if (keyspace.empty())
    {
        throw std::runtime_error(
            "nodestore: Missing keyspace in Cassandra config");
    }

    std::string tablePrefix = getString("table_prefix");
    if (tablePrefix.empty())
    {
        BOOST_LOG_TRIVIAL(warning) << "Table prefix is empty";
    }

    cass_cluster_set_connect_timeout(cluster, 10000);

    auto executeSimpleStatement = [this](std::string const& query) {
        CassStatement* statement = makeStatement(query.c_str(), 0);
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        CassError rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error executing simple statement: " << rc << ", "
               << cass_error_desc(rc) << " - " << query;
            BOOST_LOG_TRIVIAL(error) << ss.str();
            return false;
        }
        return true;
    };
    CassStatement* statement;
    CassFuture* fut;
    bool setupSessionAndTable = false;
    while (!setupSessionAndTable)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        session_.reset(cass_session_new());
        assert(session_);

        fut = cass_session_connect_keyspace(
            session_.get(), cluster, keyspace.c_str());
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error connecting Cassandra session keyspace: "
               << rc << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        std::stringstream query;
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "objects"
              << " ( key blob, sequence bigint, object blob, PRIMARY "
                 "KEY(key, "
                 "sequence)) WITH CLUSTERING ORDER BY (sequence DESC)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "objects"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE INDEX ON " << tablePrefix << "objects(sequence)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "objects WHERE sequence=1"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "transactions"
              << " ( hash blob PRIMARY KEY, ledger_sequence bigint, "
                 "transaction "
                 "blob, metadata blob)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "transactions"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE INDEX ON " << tablePrefix
              << "transactions(ledger_sequence)";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "SELECT * FROM " << tablePrefix
              << "transactions WHERE ledger_sequence = 1"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "keys"
              << " ( sequence bigint, key blob, PRIMARY KEY "
                 "(sequence, key))";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "keys"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "books"
              << " ( book blob, sequence bigint, key blob, deleted_at "
                 "bigint, PRIMARY KEY "
                 "(book, key)) WITH CLUSTERING ORDER BY (key ASC)";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "SELECT * FROM " << tablePrefix << "books"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "books2"
              << " ( book blob, sequence bigint, key blob, PRIMARY KEY "
                 "((book, sequence), key)) WITH CLUSTERING ORDER BY (key ASC)";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "SELECT * FROM " << tablePrefix << "books2"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "account_tx"
              << " ( account blob, seq_idx tuple<bigint, bigint>, "
                 " hash blob, "
                 "PRIMARY KEY "
                 "(account, seq_idx)) WITH "
                 "CLUSTERING ORDER BY (seq_idx desc)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "account_tx"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledgers"
              << " ( sequence bigint PRIMARY KEY, header blob )";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledgers"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_hashes"
              << " (hash blob PRIMARY KEY, sequence bigint)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledger_hashes"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_range"
              << " (is_latest boolean PRIMARY KEY, sequence bigint)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledger_range"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        setupSessionAndTable = true;
    }

    cass_cluster_free(cluster);

    bool setupPreparedStatements = false;
    while (!setupPreparedStatements)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::stringstream query;
        query << "INSERT INTO " << tablePrefix << "objects"
              << " (key, sequence, object) VALUES (?, ?, ?)";
        if (!insertObject_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "INSERT INTO " << tablePrefix << "transactions"
              << " (hash, ledger_sequence, transaction, metadata) VALUES "
                 "(?, ?, "
                 "?, ?)";
        if (!insertTransaction_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "INSERT INTO " << tablePrefix << "keys"
              << " (sequence, key) VALUES (?, ?)";
        if (!insertKey_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "INSERT INTO " << tablePrefix << "books"
              << " (book, key, sequence, deleted_at) VALUES (?, ?, ?, ?)";
        if (!insertBook_.prepareStatement(query, session_.get()))
            continue;
        query = {};
        query << "INSERT INTO " << tablePrefix << "books2"
              << " (book, sequence, key) VALUES (?, ?, ?)";
        if (!insertBook2_.prepareStatement(query, session_.get()))
            continue;
        query = {};
        query << "INSERT INTO " << tablePrefix << "books"
              << " (book, key, deleted_at) VALUES (?, ?, ?)";
        if (!deleteBook_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT key FROM " << tablePrefix << "keys"
              << " WHERE sequence = ? AND key > ? ORDER BY key ASC LIMIT ?";
        if (!selectKeys_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT object, sequence FROM " << tablePrefix << "objects"
              << " WHERE key = ? AND sequence <= ? ORDER BY sequence DESC "
                 "LIMIT 1";

        if (!selectObject_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT transaction, metadata, ledger_sequence FROM "
              << tablePrefix << "transactions"
              << " WHERE hash = ?";
        if (!selectTransaction_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT transaction, metadata, ledger_sequence FROM "
              << tablePrefix << "transactions"
              << " WHERE ledger_sequence = ?";
        if (!selectAllTransactionsInLedger_.prepareStatement(
                query, session_.get()))
            continue;
        query = {};
        query << "SELECT hash FROM " << tablePrefix << "transactions"
              << " WHERE ledger_sequence = ?";
        if (!selectAllTransactionHashesInLedger_.prepareStatement(
                query, session_.get()))
            continue;

        query = {};
        query << "SELECT key FROM " << tablePrefix << "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ? "
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";
        if (!selectLedgerPageKeys_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT object,key FROM " << tablePrefix << "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ? "
              << " PER PARTITION LIMIT 1 LIMIT ? ALLOW FILTERING";

        if (!selectLedgerPage_.prepareStatement(query, session_.get()))
            continue;

        /*
        query = {};
        query << "SELECT filterempty(key,object) FROM " << tablePrefix <<
        "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ?"
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";
        if (!upperBound2_.prepareStatement(query, session_.get()))
            continue;
*/
        query = {};
        query << "SELECT TOKEN(key) FROM " << tablePrefix << "objects "
              << " WHERE key = ? LIMIT 1";

        if (!getToken_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT key FROM " << tablePrefix << "books "
              << " WHERE book = ? AND sequence <= ? AND deleted_at > ? AND"
                 " key > ? "
                 " ORDER BY key ASC LIMIT ? ALLOW FILTERING";
        if (!getBook_.prepareStatement(query, session_.get()))
            continue;
        query = {};
        query << "SELECT key FROM " << tablePrefix << "books2 "
              << " WHERE book = ? AND sequence = ? AND "
                 " key > ? "
                 " ORDER BY key ASC LIMIT ?";
        if (!selectBook_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "account_tx"
              << " (account, seq_idx, hash) "
              << " VALUES (?,?,?)";
        if (!insertAccountTx_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
              << " WHERE account = ? "
              << " AND seq_idx < ? LIMIT ?";
        if (!selectAccountTx_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "ledgers "
              << " (sequence, header) VALUES(?,?)";
        if (!insertLedgerHeader_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "ledger_hashes"
              << " (hash, sequence) VALUES(?,?)";
        if (!insertLedgerHash_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " update " << tablePrefix << "ledger_range"
              << " set sequence = ? where is_latest = ? if sequence in "
                 "(?,null)";
        if (!updateLedgerRange_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " select header from " << tablePrefix
              << "ledgers where sequence = ?";
        if (!selectLedgerBySeq_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " select sequence from " << tablePrefix
              << "ledger_range where is_latest = true";
        if (!selectLatestLedger_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " SELECT sequence FROM " << tablePrefix
              << "ledger_range WHERE "
              << " is_latest IN (true, false)";
        if (!selectLedgerRange_.prepareStatement(query, session_.get()))
            continue;
        query = {};
        query << " SELECT key,object FROM " << tablePrefix
              << "objects WHERE sequence = ?";
        if (!selectLedgerDiff_.prepareStatement(query, session_.get()))
            continue;

        setupPreparedStatements = true;
    }

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!fetchLatestLedgerSequence())
        {
            std::stringstream query;
            query << "TRUNCATE TABLE " << tablePrefix << "ledger_range";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "ledgers";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "ledger_hashes";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "objects";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "transactions";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "account_tx";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "books";
            if (!executeSimpleStatement(query.str()))
                continue;
        }
        break;
    }

    if (config_.contains("max_requests_outstanding"))
    {
        maxRequestsOutstanding = config_["max_requests_outstanding"].as_int64();
    }
    if (config_.contains("run_indexer"))
    {
        if (config_["run_indexer"].as_bool())
        {
            if (config_.contains("indexer_shift"))
            {
                indexerShift_ = config_["indexer_shift"].as_int64();
            }
            indexer_ = std::thread{[this]() {
                auto rng = fetchLedgerRange();
                if (rng)
                {
                    BOOST_LOG_TRIVIAL(info) << "Running indexer. Ledger = "
                                            << std::to_string(rng->minSequence);
                    runIndexer(rng->minSequence);
                    BOOST_LOG_TRIVIAL(info) << "Ran indexer";
                }
            }};
        }
    }

    work_.emplace(ioContext_);
    ioThread_ = std::thread{[this]() { ioContext_.run(); }};
    open_ = true;

    BOOST_LOG_TRIVIAL(info) << "Opened database successfully";
}
}  // namespace Backend
