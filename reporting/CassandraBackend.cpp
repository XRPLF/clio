#include <reporting/CassandraBackend.h>
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

    CassandraResult result = executeSyncRead(statement);

    if (!result)
        return {};
    std::vector<LedgerObject> objects;
    do
    {
        objects.push_back({result.getUInt256(), result.getBytes()});
    } while (result.nextRow());
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
        ripple::uint256&& key,
        uint32_t ledgerSequence,
        std::condition_variable& cv,
        std::mutex& mtx,
        std::atomic_uint32_t& numRemaining)
        : backend(backend)
        , key(std::move(key))
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
CassandraBackend::writeKeys(uint32_t ledgerSequence) const
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
    std::vector<ripple::uint256> keys;
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor] =
                fetchLedgerPage(cursor, ledgerSequence, limit);
            cursor = curCursor;
            for (auto& obj : objects)
            {
                keys.push_back(std::move(obj.key));
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
        << " . Took " << (mid - start).count() / 1000000000.0;
    std::atomic_uint32_t numRemaining = keys.size();
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<std::shared_ptr<WriteKeyCallbackData>> cbs;
    cbs.reserve(keys.size());
    uint32_t concurrentLimit = maxRequestsOutstanding / 2;
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        cbs.push_back(std::make_shared<WriteKeyCallbackData>(
            *this, std::move(keys[i]), ledgerSequence, cv, mtx, numRemaining));
        writeKey(*cbs[i]);
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Submitted a write request";
        std::unique_lock<std::mutex> lck(mtx);
        BOOST_LOG_TRIVIAL(trace) << __func__ << "Got the mutex";
        cv.wait(lck, [&numRemaining, i, concurrentLimit, &keys]() {
            BOOST_LOG_TRIVIAL(trace)
                << std::to_string(i) << " " << std::to_string(numRemaining)
                << " " << std::to_string(keys.size()) << " "
                << std::to_string(concurrentLimit);
            // keys.size() - i is number submitted. keys.size() -
            // numRemaining is number completed Difference is num
            // outstanding
            return (i + 1 - (keys.size() - numRemaining)) < concurrentLimit;
        });
        if (i % 100000 == 0)
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Submitted " << std::to_string(i)
                << " write requests. Completed "
                << (keys.size() - numRemaining);
    }

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << "Wrote all keys from ledger "
        << std::to_string(ledgerSequence) << " . num keys = " << keys.size()
        << " . Took " << (end - mid).count() / 1000000000.0
        << ". Entire operation took " << (end - start).count() / 1000000000.0;
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
            indexer_ = std::thread{[this]() {
                auto seq = fetchLatestLedgerSequence();
                if (seq)
                {
                    auto base = (*seq >> indexerShift_) << indexerShift_;
                    BOOST_LOG_TRIVIAL(info)
                        << "Running indexer. Ledger = " << std::to_string(base)
                        << " latest = " << std::to_string(*seq);
                    writeKeys(base);
                    BOOST_LOG_TRIVIAL(info) << "Ran indexer";
                }
            }};
    }

    work_.emplace(ioContext_);
    ioThread_ = std::thread{[this]() { ioContext_.run(); }};
    open_ = true;

    BOOST_LOG_TRIVIAL(info) << "Opened database successfully";
}
}  // namespace Backend
