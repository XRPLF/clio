#include <backend/CassandraBackend.h>
#include <backend/DBHelpers.h>
#include <functional>
#include <unordered_map>
namespace Backend {
template <class T, class F>
void
processAsyncWriteResponse(T& requestParams, CassFuture* fut, F func)
{
    CassandraBackend const& backend = *requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
    {
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra write error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying in " << wait.count()
            << " milliseconds";
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.getIOContext(),
                std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, func](
                              const boost::system::error_code& error) {
            func(requestParams, true);
        });
    }
    else
    {
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " Succesfully inserted a record";
        requestParams.finish();
    }
}
template <class T>
void
processAsyncWrite(CassFuture* fut, void* cbData)
{
    T& requestParams = *static_cast<T*>(cbData);
    // TODO don't pass in func
    processAsyncWriteResponse(requestParams, fut, requestParams.retry);
}

template <class T, class B>
struct WriteCallbackData
{
    CassandraBackend const* backend;
    T data;
    std::function<void(WriteCallbackData<T, B>&, bool)> retry;
    uint32_t currentRetries;
    std::atomic<int> refs = 1;

    WriteCallbackData(CassandraBackend const* b, T&& d, B bind)
        : backend(b), data(std::move(d))
    {
        retry = [bind, this](auto& params, bool isRetry) {
            auto statement = bind(params);
            backend->executeAsyncWrite(
                statement,
                processAsyncWrite<
                    typename std::remove_reference<decltype(params)>::type>,
                params,
                isRetry);
        };
    }
    virtual void
    start()
    {
        retry(*this, false);
    }

    virtual void
    finish()
    {
        backend->finishAsyncWrite();
        int remaining = --refs;
        if (remaining == 0)
            delete this;
    }
    virtual ~WriteCallbackData()
    {
    }
};
template <class T, class B>
struct BulkWriteCallbackData : public WriteCallbackData<T, B>
{
    std::mutex& mtx;
    std::condition_variable& cv;
    std::atomic_int& numRemaining;
    BulkWriteCallbackData(
        CassandraBackend const* b,
        T&& d,
        B bind,
        std::atomic_int& r,
        std::mutex& m,
        std::condition_variable& c)
        : WriteCallbackData<T, B>(b, std::move(d), bind)
        , numRemaining(r)
        , mtx(m)
        , cv(c)
    {
    }
    void
    start() override
    {
        this->retry(*this, true);
    }

    void
    finish() override
    {
        // TODO: it would be nice to avoid this lock.
        std::lock_guard lck(mtx);
        if (--numRemaining == 0)
            cv.notify_one();
    }
    ~BulkWriteCallbackData()
    {
    }
};

template <class T, class B>
void
makeAndExecuteAsyncWrite(CassandraBackend const* b, T&& d, B bind)
{
    auto* cb = new WriteCallbackData(b, std::move(d), bind);
    cb->start();
}
template <class T, class B>
std::shared_ptr<BulkWriteCallbackData<T, B>>
makeAndExecuteBulkAsyncWrite(
    CassandraBackend const* b,
    T&& d,
    B bind,
    std::atomic_int& r,
    std::mutex& m,
    std::condition_variable& c)
{
    auto cb = std::make_shared<BulkWriteCallbackData<T, B>>(
        b, std::move(d), bind, r, m, c);
    cb->start();
    return cb;
}
void
CassandraBackend::doWriteLedgerObject(
    std::string&& key,
    uint32_t seq,
    std::string&& blob) const
{
    BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
    makeAndExecuteAsyncWrite(
        this,
        std::move(std::make_tuple(std::move(key), seq, std::move(blob))),
        [this](auto& params) {
            auto& [key, sequence, blob] = params.data;

            CassandraStatement statement{insertObject_};
            statement.bindNextBytes(key);
            statement.bindNextInt(sequence);
            statement.bindNextBytes(blob);
            return statement;
        });
}
void
CassandraBackend::writeLedger(
    ripple::LedgerInfo const& ledgerInfo,
    std::string&& header,
    bool isFirst) const
{
    makeAndExecuteAsyncWrite(
        this,
        std::move(std::make_tuple(ledgerInfo.seq, std::move(header))),
        [this](auto& params) {
            auto& [sequence, header] = params.data;
            CassandraStatement statement{insertLedgerHeader_};
            statement.bindNextInt(sequence);
            statement.bindNextBytes(header);
            return statement;
        });
    makeAndExecuteAsyncWrite(
        this,
        std::move(std::make_tuple(ledgerInfo.hash, ledgerInfo.seq)),
        [this](auto& params) {
            auto& [hash, sequence] = params.data;
            CassandraStatement statement{insertLedgerHash_};
            statement.bindNextBytes(hash);
            statement.bindNextInt(sequence);
            return statement;
        });
    ledgerSequence_ = ledgerInfo.seq;
    isFirstLedger_ = isFirst;
}
void
CassandraBackend::writeAccountTransactions(
    std::vector<AccountTransactionsData>&& data) const
{
    for (auto& record : data)
    {
        for (auto& account : record.accounts)
        {
            makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(
                    std::move(account),
                    record.ledgerSequence,
                    record.transactionIndex,
                    record.txHash)),
                [this](auto& params) {
                    CassandraStatement statement(insertAccountTx_);
                    auto& [account, lgrSeq, txnIdx, hash] = params.data;
                    statement.bindNextBytes(account);
                    uint32_t index = lgrSeq >> 20 << 20;
                    statement.bindNextUInt(index);

                    statement.bindNextIntTuple(lgrSeq, txnIdx);
                    statement.bindNextBytes(hash);
                    return statement;
                });
        }
    }
}
void
CassandraBackend::writeTransaction(
    std::string&& hash,
    uint32_t seq,
    uint32_t date,
    std::string&& transaction,
    std::string&& metadata) const
{
    BOOST_LOG_TRIVIAL(trace) << "Writing txn to cassandra";
    std::string hashCpy = hash;

    makeAndExecuteAsyncWrite(
        this, std::move(std::make_pair(seq, hash)), [this](auto& params) {
            CassandraStatement statement{insertLedgerTransaction_};
            statement.bindNextInt(params.data.first);
            statement.bindNextBytes(params.data.second);
            return statement;
        });
    makeAndExecuteAsyncWrite(
        this,
        std::move(std::make_tuple(
            std::move(hash),
            seq,
            date,
            std::move(transaction),
            std::move(metadata))),
        [this](auto& params) {
            CassandraStatement statement{insertTransaction_};
            auto& [hash, sequence, date, transaction, metadata] = params.data;
            statement.bindNextBytes(hash);
            statement.bindNextInt(sequence);
            statement.bindNextInt(date);
            statement.bindNextBytes(transaction);
            statement.bindNextBytes(metadata);
            return statement;
        });
}

std::optional<LedgerRange>
CassandraBackend::hardFetchLedgerRange() const
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
    auto hashes = fetchAllTransactionHashesInLedger(ledgerSequence);
    return fetchTransactions(hashes);
}

struct ReadCallbackData
{
    std::function<void(CassandraResult&)> onSuccess;
    std::atomic_int& numOutstanding;
    std::mutex& mtx;
    std::condition_variable& cv;
    std::atomic_bool errored = false;
    ReadCallbackData(
        std::atomic_int& numOutstanding,
        std::mutex& m,
        std::condition_variable& cv,
        std::function<void(CassandraResult&)> onSuccess)
        : numOutstanding(numOutstanding), mtx(m), cv(cv), onSuccess(onSuccess)
    {
    }

    void
    finish(CassFuture* fut)
    {
        CassError rc = cass_future_error_code(fut);
        if (rc != CASS_OK)
        {
            errored = true;
        }
        else
        {
            CassandraResult result{cass_future_get_result(fut)};
            onSuccess(result);
        }
        std::lock_guard lck(mtx);
        if (--numOutstanding == 0)
            cv.notify_one();
    }
};
void
processAsyncRead(CassFuture* fut, void* cbData)
{
    ReadCallbackData& cb = *static_cast<ReadCallbackData*>(cbData);
    cb.finish(fut);
}
std::vector<TransactionAndMetadata>
CassandraBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes) const
{
    std::size_t const numHashes = hashes.size();
    std::atomic_int numOutstanding = numHashes;
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<TransactionAndMetadata> results{numHashes};
    std::vector<std::shared_ptr<ReadCallbackData>> cbs;
    cbs.reserve(numHashes);
    auto start = std::chrono::system_clock::now();
    for (std::size_t i = 0; i < hashes.size(); ++i)
    {
        CassandraStatement statement{selectTransaction_};
        statement.bindNextBytes(hashes[i]);
        cbs.push_back(std::make_shared<ReadCallbackData>(
            numOutstanding, mtx, cv, [i, &results](auto& result) {
                if (result.hasResult())
                    results[i] = {
                        result.getBytes(),
                        result.getBytes(),
                        result.getUInt32(),
                        result.getUInt32()};
            }));
        executeAsyncRead(statement, processAsyncRead, *cbs[i]);
    }
    assert(results.size() == cbs.size());

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
    auto end = std::chrono::system_clock::now();
    for (auto const& cb : cbs)
    {
        if (cb->errored)
            throw DatabaseTimeout();
    }

    BOOST_LOG_TRIVIAL(debug)
        << "Fetched " << numHashes << " transactions from Cassandra in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
               .count()
        << " milliseconds";
    return results;
}
std::vector<ripple::uint256>
CassandraBackend::fetchAllTransactionHashesInLedger(
    uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectAllTransactionHashesInLedger_};
    statement.bindNextInt(ledgerSequence);
    auto start = std::chrono::system_clock::now();
    CassandraResult result = executeSyncRead(statement);
    auto end = std::chrono::system_clock::now();
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__
            << " - no rows . ledger = " << std::to_string(ledgerSequence);
        return {};
    }
    std::vector<ripple::uint256> hashes;
    do
    {
        hashes.push_back(result.getUInt256());
    } while (result.nextRow());
    BOOST_LOG_TRIVIAL(debug)
        << "Fetched " << hashes.size()
        << " transaction hashes from Cassandra in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
               .count()
        << " milliseconds";
    return hashes;
}

AccountTransactions
CassandraBackend::fetchAccountTransactions(
    ripple::AccountID const& account,
    std::uint32_t limit,
    bool forward,
    std::optional<AccountTransactionsCursor> const& cursorIn) const
{
    auto rng = fetchLedgerRange();
    if (!rng)
        return {{}, {}};
    std::pair<
        std::vector<TransactionAndMetadata>,
        std::optional<AccountTransactionsCursor>>
        res;
    auto keylet = ripple::keylet::account(account);
    std::vector<ripple::uint256> hashes;
    auto cursor = cursorIn;
    do
    {
        {
            CassandraStatement statement = [this, forward]() {
                if (forward)
                    return CassandraStatement{selectAccountTxForward_};
                else
                    return CassandraStatement{selectAccountTx_};
            }();
            statement.bindNextBytes(account);
            if (cursor)
            {
                statement.bindNextUInt(cursor->ledgerSequence >> 20 << 20);
                statement.bindNextIntTuple(
                    cursor->ledgerSequence, cursor->transactionIndex);
                BOOST_LOG_TRIVIAL(debug)
                    << " account = " << ripple::strHex(account)
                    << " idx = " << (cursor->ledgerSequence >> 20 << 20)
                    << " tuple = " << cursor->ledgerSequence << " : "
                    << cursor->transactionIndex;
            }
            else
            {
                int seq = forward ? rng->minSequence : rng->maxSequence;
                statement.bindNextUInt(seq >> 20 << 20);
                int placeHolder = forward ? 0 : INT32_MAX;

                statement.bindNextIntTuple(placeHolder, placeHolder);
                BOOST_LOG_TRIVIAL(debug)
                    << " account = " << ripple::strHex(account)
                    << " idx = " << seq << " tuple = " << placeHolder;
            }
            uint32_t adjustedLimit = limit - hashes.size();
            statement.bindNextUInt(adjustedLimit);
            CassandraResult result = executeSyncRead(statement);
            if (!result.hasResult())
            {
                BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
                break;
            }

            size_t numRows = result.numRows();

            BOOST_LOG_TRIVIAL(info) << "num_rows = " << std::to_string(numRows);
            do
            {
                hashes.push_back(result.getUInt256());
                --numRows;
                if (numRows == 0)
                {
                    BOOST_LOG_TRIVIAL(debug) << __func__ << " setting cursor";
                    auto [lgrSeq, txnIdx] = result.getInt64Tuple();
                    cursor = {(uint32_t)lgrSeq, (uint32_t)txnIdx};
                    if (forward)
                        ++cursor->transactionIndex;
                }
            } while (result.nextRow());
        }
        if (hashes.size() < limit)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " less than limit";
            uint32_t seq = cursor->ledgerSequence;
            seq = seq >> 20;
            if (forward)
                seq += 1;
            else
                seq -= 1;
            seq = seq << 20;
            cursor->ledgerSequence = seq;
            cursor->transactionIndex = forward ? 0 : INT32_MAX;
            BOOST_LOG_TRIVIAL(debug) << __func__ << " walking back";
            CassandraStatement statement{selectObject_};
            statement.bindNextBytes(keylet.key);
            statement.bindNextInt(seq);
            CassandraResult result = executeSyncRead(statement);
            if (!result)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " account no longer exists";
                cursor = {};
                break;
            }
        }
    } while (hashes.size() < limit &&
             cursor->ledgerSequence >= rng->minSequence);

    auto txns = fetchTransactions(hashes);
    BOOST_LOG_TRIVIAL(debug) << __func__ << "txns = " << txns.size();
    if (txns.size() >= limit)
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " returning cursor";
        return {txns, cursor};
    }
    return {txns, {}};
}

LedgerPage
CassandraBackend::doFetchLedgerPage(
    std::optional<ripple::uint256> const& cursorIn,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    std::optional<ripple::uint256> cursor = cursorIn;
    auto index = getKeyIndexOfSeq(ledgerSequence);
    if (!index)
        return {};
    LedgerPage page;
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " ledgerSequence = " << std::to_string(ledgerSequence)
        << " index = " << std::to_string(index->keyIndex);
    if (cursor)
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " - Cursor = " << ripple::strHex(*cursor);
    CassandraStatement statement{selectKeys_};
    statement.bindNextInt(index->keyIndex);
    if (!cursor)
    {
        ripple::uint256 zero;
        cursor = zero;
    }
    statement.bindNextBytes(cursor->data(), 1);
    statement.bindNextBytes(*cursor);
    statement.bindNextUInt(limit + 1);
    CassandraResult result = executeSyncRead(statement);
    if (!!result)
    {
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " - got keys - size = " << result.numRows();

        std::vector<ripple::uint256> keys;
        do
        {
            keys.push_back(result.getUInt256());
        } while (result.nextRow());

        if (keys.size() && keys.size() >= limit)
        {
            page.cursor = keys.back();
            ++(*page.cursor);
        }
        else if (cursor->data()[0] != 0xFF)
        {
            ripple::uint256 zero;
            zero.data()[0] = cursor->data()[0] + 1;
            page.cursor = zero;
        }
        auto objects = fetchLedgerObjects(keys, ledgerSequence);
        if (objects.size() != keys.size())
            throw std::runtime_error("Mismatch in size of objects and keys");

        if (cursor)
            BOOST_LOG_TRIVIAL(debug)
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

        if (!cursor && (!keys.size() || !keys[0].isZero()))
        {
            page.warning = "Data may be incomplete";
        }

        return page;
    }
    if (!cursor)
        return {{}, {}, "Data may be incomplete"};

    return {};
}
std::vector<Blob>
CassandraBackend::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    uint32_t sequence) const
{
    std::size_t const numKeys = keys.size();
    BOOST_LOG_TRIVIAL(trace)
        << "Fetching " << numKeys << " records from Cassandra";
    std::atomic_int numOutstanding = numKeys;
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<Blob> results{numKeys};
    std::vector<std::shared_ptr<ReadCallbackData>> cbs;
    cbs.reserve(numKeys);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        cbs.push_back(std::make_shared<ReadCallbackData>(
            numOutstanding, mtx, cv, [i, &results](auto& result) {
                if (result.hasResult())
                    results[i] = result.getBytes();
            }));
        CassandraStatement statement{selectObject_};
        statement.bindNextBytes(keys[i]);
        statement.bindNextInt(sequence);
        executeAsyncRead(statement, processAsyncRead, *cbs[i]);
    }
    assert(results.size() == cbs.size());

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
    for (auto const& cb : cbs)
    {
        if (cb->errored)
            throw DatabaseTimeout();
    }

    BOOST_LOG_TRIVIAL(trace)
        << "Fetched " << numKeys << " records from Cassandra";
    return results;
}

bool
CassandraBackend::writeKeys(
    std::unordered_set<ripple::uint256> const& keys,
    KeyIndex const& index,
    bool isAsync) const
{
    auto bind = [this](auto& params) {
        auto& [lgrSeq, key] = params.data;
        CassandraStatement statement{insertKey_};
        statement.bindNextInt(lgrSeq);
        statement.bindNextBytes(key.data(), 1);
        statement.bindNextBytes(key);
        return statement;
    };
    std::atomic_int numOutstanding = 0;
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<std::shared_ptr<BulkWriteCallbackData<
        std::pair<uint32_t, ripple::uint256>,
        typename std::remove_reference<decltype(bind)>::type>>>
        cbs;
    cbs.reserve(keys.size());
    uint32_t concurrentLimit =
        isAsync ? indexerMaxRequestsOutstanding : maxRequestsOutstanding;
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " Ledger = " << std::to_string(index.keyIndex)
        << " . num keys = " << std::to_string(keys.size())
        << " . concurrentLimit = "
        << std::to_string(indexerMaxRequestsOutstanding);
    uint32_t numSubmitted = 0;
    for (auto& key : keys)
    {
        cbs.push_back(makeAndExecuteBulkAsyncWrite(
            this,
            std::make_pair(index.keyIndex, std::move(key)),
            bind,
            numOutstanding,
            mtx,
            cv));
        ++numOutstanding;
        ++numSubmitted;
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [&numOutstanding, concurrentLimit, &keys]() {
            // keys.size() - i is number submitted. keys.size() -
            // numRemaining is number completed Difference is num
            // outstanding
            return numOutstanding < concurrentLimit;
        });
        if (numSubmitted % 100000 == 0)
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " Submitted " << std::to_string(numSubmitted);
    }

    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
    return true;
}

bool
CassandraBackend::doOnlineDelete(uint32_t numLedgersToKeep) const
{
    // calculate TTL
    // ledgers close roughly every 4 seconds. We double the TTL so that way
    // there is a window of time to update the database, to prevent unchanging
    // records from being deleted.
    auto rng = fetchLedgerRange();
    if (!rng)
        return false;
    uint32_t minLedger = rng->maxSequence - numLedgersToKeep;
    if (minLedger <= rng->minSequence)
        return false;
    auto bind = [this](auto& params) {
        auto& [key, seq, obj] = params.data;
        CassandraStatement statement{insertObject_};
        statement.bindNextBytes(key);
        statement.bindNextInt(seq);
        statement.bindNextBytes(obj);
        return statement;
    };
    std::condition_variable cv;
    std::mutex mtx;
    std::vector<std::shared_ptr<BulkWriteCallbackData<
        std::tuple<ripple::uint256, uint32_t, Blob>,
        typename std::remove_reference<decltype(bind)>::type>>>
        cbs;
    uint32_t concurrentLimit = 10;
    std::atomic_int numOutstanding = 0;

    // iterate through latest ledger, updating TTL
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                fetchLedgerPage(cursor, minLedger, 256);
            if (warning)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__
                    << " online delete running but flag ledger is not complete";
                std::this_thread::sleep_for(std::chrono::seconds(10));
                continue;
            }

            for (auto& obj : objects)
            {
                ++numOutstanding;
                cbs.push_back(makeAndExecuteBulkAsyncWrite(
                    this,
                    std::make_tuple(
                        std::move(obj.key), minLedger, std::move(obj.blob)),
                    bind,
                    numOutstanding,
                    mtx,
                    cv));

                std::unique_lock<std::mutex> lck(mtx);
                BOOST_LOG_TRIVIAL(trace) << __func__ << "Got the mutex";
                cv.wait(lck, [&numOutstanding, concurrentLimit]() {
                    return numOutstanding < concurrentLimit;
                });
            }
            BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
            cursor = curCursor;
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
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
    CassandraStatement statement{deleteLedgerRange_};
    statement.bindNextInt(minLedger);
    executeSyncWrite(statement);
    // update ledger_range
    return true;
}

void
CassandraBackend::open(bool readOnly)
{
    auto getString = [this](std::string const& field) -> std::string {
        if (config_.contains(field))
        {
            auto jsonStr = config_[field].as_string();
            return {jsonStr.c_str(), jsonStr.size()};
        }
        return {""};
    };
    auto getInt = [this](std::string const& field) -> std::optional<int> {
        if (config_.contains(field) && config_.at(field).is_int64())
            return config_[field].as_int64();
        return {};
    };
    if (open_)
    {
        assert(false);
        BOOST_LOG_TRIVIAL(error) << "database is already open";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "Opening Cassandra Backend";

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

        auto port = getInt("port");
        if (port)
        {
            rc = cass_cluster_set_port(cluster, *port);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra port: " << *port
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
    int threads = getInt("threads") ? *getInt("threads")
                                    : std::thread::hardware_concurrency();

    rc = cass_cluster_set_num_threads_io(cluster, threads);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra io threads to " << threads
           << ", result: " << rc << ", " << cass_error_desc(rc);
        throw std::runtime_error(ss.str());
    }
    if (getInt("max_requests_outstanding"))
        maxRequestsOutstanding = *getInt("max_requests_outstanding");

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
        BOOST_LOG_TRIVIAL(warning)
            << "No keyspace specified. Using keyspace oceand";
        keyspace = "oceand";
    }

    int rf = getInt("replication_factor") ? *getInt("replication_factor") : 3;

    std::string tablePrefix = getString("table_prefix");
    if (tablePrefix.empty())
    {
        BOOST_LOG_TRIVIAL(warning) << "Table prefix is empty";
    }

    cass_cluster_set_connect_timeout(cluster, 10000);

    int ttl = getInt("ttl") ? *getInt("ttl") * 2 : 0;
    int keysTtl = (ttl != 0 ? pow(2, indexer_.getKeyShift()) * 4 * 2 : 0);
    int incr = keysTtl;
    while (keysTtl < ttl)
    {
        keysTtl += incr;
    }
    int booksTtl = 0;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " setting ttl to " << std::to_string(ttl)
        << " , books ttl to " << std::to_string(booksTtl) << " , keys ttl to "
        << std::to_string(keysTtl);

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
               << rc << ", " << cass_error_desc(rc)
               << ", trying to create it ourselves";
            BOOST_LOG_TRIVIAL(error) << ss.str();
            // if the keyspace doesn't exist, try to create it
            session_.reset(cass_session_new());
            fut = cass_session_connect(session_.get(), cluster);
            rc = cass_future_error_code(fut);
            cass_future_free(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "nodestore: Error connecting Cassandra session at all: "
                   << rc << ", " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
            }
            else
            {
                std::stringstream query;
                query << "CREATE KEYSPACE IF NOT EXISTS " << keyspace
                      << " WITH replication = {'class': 'SimpleStrategy', "
                         "'replication_factor': '"
                      << std::to_string(rf) << "'}  AND durable_writes = true";
                if (!executeSimpleStatement(query.str()))
                    continue;
                query.str("");
                query << "USE " << keyspace;
                if (!executeSimpleStatement(query.str()))
                    continue;
            }

            continue;
        }

        std::stringstream query;
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "objects"
              << " ( key blob, sequence bigint, object blob, PRIMARY "
                 "KEY(key, "
                 "sequence)) WITH CLUSTERING ORDER BY (sequence DESC) AND"
              << " default_time_to_live = " << std::to_string(ttl);
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "objects"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query
            << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "transactions"
            << " ( hash blob PRIMARY KEY, ledger_sequence bigint, date bigint, "
               "transaction blob, metadata blob)"
            << " WITH default_time_to_live = " << std::to_string(ttl);
        if (!executeSimpleStatement(query.str()))
            continue;
        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix
              << "ledger_transactions"
              << " ( ledger_sequence bigint, hash blob, PRIMARY "
                 "KEY(ledger_sequence, hash))"
              << " WITH default_time_to_live = " << std::to_string(ttl);
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "transactions"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query.str("");
        query << "SELECT * FROM " << tablePrefix << "ledger_transactions"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "keys"
              << " ( sequence bigint, first_byte blob, key blob, PRIMARY KEY "
                 "((sequence,first_byte), key))"
                 " WITH default_time_to_live = "
              << std::to_string(keysTtl);
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "keys"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "account_tx"
              << " ( account blob, idx int, seq_idx "
                 "tuple<bigint, bigint>, "
                 " hash blob, "
                 "PRIMARY KEY "
                 "((account,idx), seq_idx)) WITH "
                 "CLUSTERING ORDER BY (seq_idx desc)"
              << " AND default_time_to_live = " << std::to_string(ttl);

        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "account_tx"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledgers"
              << " ( sequence bigint PRIMARY KEY, header blob )"
              << " WITH default_time_to_live = " << std::to_string(ttl);
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "ledgers"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_hashes"
              << " (hash blob PRIMARY KEY, sequence bigint)"
              << " WITH default_time_to_live = " << std::to_string(ttl);
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "ledger_hashes"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_range"
              << " (is_latest boolean PRIMARY KEY, sequence bigint)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
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

        query.str("");
        query << "INSERT INTO " << tablePrefix << "transactions"
              << " (hash, ledger_sequence, date, transaction, metadata) VALUES "
                 "(?, ?, ?, ?, ?)";
        if (!insertTransaction_.prepareStatement(query, session_.get()))
            continue;
        query.str("");
        query << "INSERT INTO " << tablePrefix << "ledger_transactions"
              << " (ledger_sequence, hash) VALUES "
                 "(?, ?)";
        if (!insertLedgerTransaction_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "INSERT INTO " << tablePrefix << "keys"
              << " (sequence,first_byte, key) VALUES (?, ?, ?)";
        if (!insertKey_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT key FROM " << tablePrefix << "keys"
              << " WHERE sequence = ? AND first_byte = ? AND key >= ? ORDER BY "
                 "key ASC LIMIT ?";
        if (!selectKeys_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT object, sequence FROM " << tablePrefix << "objects"
              << " WHERE key = ? AND sequence <= ? ORDER BY sequence DESC "
                 "LIMIT 1";

        if (!selectObject_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT transaction, metadata, ledger_sequence, date FROM "
              << tablePrefix << "transactions"
              << " WHERE hash = ?";
        if (!selectTransaction_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT hash FROM " << tablePrefix << "ledger_transactions"
              << " WHERE ledger_sequence = ?";
        if (!selectAllTransactionHashesInLedger_.prepareStatement(
                query, session_.get()))
            continue;

        query.str("");
        query << "SELECT key FROM " << tablePrefix << "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ? "
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";
        if (!selectLedgerPageKeys_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT object,key FROM " << tablePrefix << "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ? "
              << " PER PARTITION LIMIT 1 LIMIT ? ALLOW FILTERING";

        if (!selectLedgerPage_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT TOKEN(key) FROM " << tablePrefix << "objects "
              << " WHERE key = ? LIMIT 1";

        if (!getToken_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " INSERT INTO " << tablePrefix << "account_tx"
              << " (account, idx, seq_idx, hash) "
              << " VALUES (?,?,?,?)";
        if (!insertAccountTx_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
              << " WHERE account = ? "
              << " AND idx = ? "
              << " AND seq_idx < ? LIMIT ?";
        if (!selectAccountTx_.prepareStatement(query, session_.get()))
            continue;
        query.str("");
        query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
              << " WHERE account = ? "
              << " AND idx = ? "
              << " AND seq_idx >= ? ORDER BY seq_idx ASC LIMIT ?";
        if (!selectAccountTxForward_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " INSERT INTO " << tablePrefix << "ledgers "
              << " (sequence, header) VALUES(?,?)";
        if (!insertLedgerHeader_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " INSERT INTO " << tablePrefix << "ledger_hashes"
              << " (hash, sequence) VALUES(?,?)";
        if (!insertLedgerHash_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT sequence FROM " << tablePrefix << "ledger_hashes "
              << "WHERE hash = ? LIMIT 1";
        if (!selectLedgerByHash_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " update " << tablePrefix << "ledger_range"
              << " set sequence = ? where is_latest = ? if sequence in "
                 "(?,null)";
        if (!updateLedgerRange_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " update " << tablePrefix << "ledger_range"
              << " set sequence = ? where is_latest = false";
        if (!deleteLedgerRange_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " select header from " << tablePrefix
              << "ledgers where sequence = ?";
        if (!selectLedgerBySeq_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " select sequence from " << tablePrefix
              << "ledger_range where is_latest = true";
        if (!selectLatestLedger_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << " SELECT sequence FROM " << tablePrefix << "ledger_range";
        if (!selectLedgerRange_.prepareStatement(query, session_.get()))
            continue;
        setupPreparedStatements = true;
    }

    work_.emplace(ioContext_);
    ioThread_ = std::thread{[this]() { ioContext_.run(); }};
    open_ = true;

    BOOST_LOG_TRIVIAL(info) << "Opened CassandraBackend successfully";
}  // namespace Backend
}  // namespace Backend
