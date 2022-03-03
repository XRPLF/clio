#include <backend/CassandraBackend.h>
#include <backend/DBHelpers.h>
#include <functional>
#include <unordered_map>

namespace Backend {

// Type alias for async completion handlers
    using completion_token = boost::asio::yield_context;
    using function_type = void(boost::system::error_code);
    using result_type = boost::asio::async_result<completion_token, function_type>;
    using handler_type = typename result_type::completion_handler_type;

    template<class T, class F>
    void
    processAsyncWriteResponse(T &requestParams, CassFuture *fut, F func) {
        CassandraBackend const &backend = *requestParams.backend;
        auto rc = cass_future_error_code(fut);
        if (rc != CASS_OK) {
            // exponential backoff with a max wait of 2^10 ms (about 1 second)
            auto wait = std::chrono::milliseconds(
                    lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
            BOOST_LOG_TRIVIAL(error)
                << "ERROR!!! Cassandra write error: " << rc << ", "
                << cass_error_desc(rc) << " id= " << requestParams.toString()
                << ", retrying in " << wait.count() << " milliseconds";
            ++requestParams.currentRetries;
            std::shared_ptr<boost::asio::steady_timer> timer =
                    std::make_shared<boost::asio::steady_timer>(
                            backend.getIOContext(),
                            std::chrono::steady_clock::now() + wait);
            timer->async_wait([timer, &requestParams, func](
                    const boost::system::error_code &error) {
                func(requestParams, true);
            });
        } else {
            BOOST_LOG_TRIVIAL(trace)
                << __func__ << " Succesfully inserted a record";
            requestParams.finish();
        }
    }

    template<class T>
    void
    processAsyncWrite(CassFuture *fut, void *cbData) {
        T &requestParams = *static_cast<T *>(cbData);
        // TODO don't pass in func
        processAsyncWriteResponse(requestParams, fut, requestParams.retry);
    }

    template<class T, class B>
    struct WriteCallbackData {
        CassandraBackend const *backend;
        T data;
        std::function<void(WriteCallbackData<T, B> &, bool)> retry;
        std::uint32_t currentRetries;
        std::atomic<int> refs = 1;
        std::string id;

        WriteCallbackData(
                CassandraBackend const *b,
                T &&d,
                B bind,
                std::string const &identifier)
                : backend(b), data(std::move(d)), id(identifier) {
            retry = [bind, this](auto &params, bool isRetry) {
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
        start() {
            retry(*this, false);
        }

        virtual void
        finish() {
            backend->finishAsyncWrite();
            int remaining = --refs;
            if (remaining == 0)
                delete this;
        }

        virtual ~WriteCallbackData() {
        }

        std::string
        toString() {
            return id;
        }
    };

    template<class T, class B>
    struct BulkWriteCallbackData : public WriteCallbackData<T, B> {
        std::atomic_int &numRemaining;
        std::mutex &mtx;
        std::condition_variable &cv;

        BulkWriteCallbackData(
                CassandraBackend const *b,
                T &&d,
                B bind,
                std::atomic_int &r,
                std::mutex &m,
                std::condition_variable &c)
                : WriteCallbackData<T, B>(b, std::move(d), bind, "bulk"), numRemaining(r), mtx(m), cv(c) {
        }

        void
        start() override {
            this->retry(*this, true);
        }

        void
        finish() override {
            // TODO: it would be nice to avoid this lock.
            std::lock_guard lck(mtx);
            if (--numRemaining == 0)
                cv.notify_one();
        }

        ~BulkWriteCallbackData() {
        }
    };

    template<class T, class B>
    void
    makeAndExecuteAsyncWrite(
            CassandraBackend const *b,
            T &&d,
            B bind,
            std::string const &id) {
        auto *cb = new WriteCallbackData<T, B>(b, std::move(d), bind, id);
        cb->start();
    }

    template<class T, class B>
    std::shared_ptr<BulkWriteCallbackData<T, B>>
    makeAndExecuteBulkAsyncWrite(
            CassandraBackend const *b,
            T &&d,
            B bind,
            std::atomic_int &r,
            std::mutex &m,
            std::condition_variable &c) {
        auto cb = std::make_shared<BulkWriteCallbackData<T, B>>(
                b, std::move(d), bind, r, m, c);
        cb->start();
        return cb;
    }

    void
    CassandraBackend::doWriteLedgerObject(
            std::string &&key,
            std::uint32_t const seq,
            std::string &&blob) {
        BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
        if (range)
            makeAndExecuteAsyncWrite(
                    this,
                    std::move(std::make_tuple(seq, key)),
                    [this](auto &params) {
                        auto&[sequence, key] = params.data;

                        CassandraStatement statement{insertDiff_};
                        statement.bindNextInt(sequence);
                        statement.bindNextBytes(key);
                        return statement;
                    },
                    "ledger_diff");
        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(std::move(key), seq, std::move(blob))),
                [this](auto &params) {
                    auto&[key, sequence, blob] = params.data;

                    CassandraStatement statement{insertObject_};
                    statement.bindNextBytes(key);
                    statement.bindNextInt(sequence);
                    statement.bindNextBytes(blob);
                    return statement;
                },
                "ledger_object");
    }

    void
    CassandraBackend::writeSuccessor(
            std::string &&key,
            std::uint32_t const seq,
            std::string &&successor) {
        BOOST_LOG_TRIVIAL(trace)
            << "Writing successor. key = " << key
            << " seq = " << std::to_string(seq) << " successor = " << successor;
        assert(key.size() != 0);
        assert(successor.size() != 0);
        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(std::move(key), seq, std::move(successor))),
                [this](auto &params) {
                    auto&[key, sequence, successor] = params.data;

                    CassandraStatement statement{insertSuccessor_};
                    statement.bindNextBytes(key);
                    statement.bindNextInt(sequence);
                    statement.bindNextBytes(successor);
                    return statement;
                },
                "successor");
    }

    void
    CassandraBackend::writeLedger(
            ripple::LedgerInfo const &ledgerInfo,
            std::string &&header) {
        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(ledgerInfo.seq, std::move(header))),
                [this](auto &params) {
                    auto&[sequence, header] = params.data;
                    CassandraStatement statement{insertLedgerHeader_};
                    statement.bindNextInt(sequence);
                    statement.bindNextBytes(header);
                    return statement;
                },
                "ledger");
        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(ledgerInfo.hash, ledgerInfo.seq)),
                [this](auto &params) {
                    auto&[hash, sequence] = params.data;
                    CassandraStatement statement{insertLedgerHash_};
                    statement.bindNextBytes(hash);
                    statement.bindNextInt(sequence);
                    return statement;
                },
                "ledger_hash");
        ledgerSequence_ = ledgerInfo.seq;
    }

    void
    CassandraBackend::writeAccountTransactions(
            std::vector<AccountTransactionsData> &&data) {
        for (auto &record: data) {
            for (auto &account: record.accounts) {
                makeAndExecuteAsyncWrite(
                        this,
                        std::move(std::make_tuple(
                                std::move(account),
                                record.ledgerSequence,
                                record.transactionIndex,
                                record.txHash)),
                        [this](auto &params) {
                            CassandraStatement statement(insertAccountTx_);
                            auto&[account, lgrSeq, txnIdx, hash] = params.data;
                            statement.bindNextBytes(account);
                            statement.bindNextIntTuple(lgrSeq, txnIdx);
                            statement.bindNextBytes(hash);
                            return statement;
                        },
                        "account_tx");
            }
        }
    }

    void
    CassandraBackend::writeTransaction(
            std::string &&hash,
            std::uint32_t const seq,
            std::uint32_t const date,
            std::string &&transaction,
            std::string &&metadata) {
        BOOST_LOG_TRIVIAL(trace) << "Writing txn to cassandra";
        std::string hashCpy = hash;

        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_pair(seq, hash)),
                [this](auto &params) {
                    CassandraStatement statement{insertLedgerTransaction_};
                    statement.bindNextInt(params.data.first);
                    statement.bindNextBytes(params.data.second);
                    return statement;
                },
                "ledger_transaction");
        makeAndExecuteAsyncWrite(
                this,
                std::move(std::make_tuple(
                        std::move(hash),
                        seq,
                        date,
                        std::move(transaction),
                        std::move(metadata))),
                [this](auto &params) {
                    CassandraStatement statement{insertTransaction_};
                    auto&[hash, sequence, date, transaction, metadata] = params.data;
                    statement.bindNextBytes(hash);
                    statement.bindNextInt(sequence);
                    statement.bindNextInt(date);
                    statement.bindNextBytes(transaction);
                    statement.bindNextBytes(metadata);
                    return statement;
                },
                "transaction");
    }

    std::optional<LedgerRange>
    CassandraBackend::hardFetchLedgerRange(boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{selectLedgerRange_};
        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result) {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        LedgerRange range;
        range.maxSequence = range.minSequence = result.getUInt32();
        if (result.nextRow()) {
            range.maxSequence = result.getUInt32();
        }
        if (range.minSequence > range.maxSequence) {
            std::swap(range.minSequence, range.maxSequence);
        }
        return range;
    }

    std::vector<TransactionAndMetadata>
    CassandraBackend::fetchAllTransactionsInLedger(
            std::uint32_t const ledgerSequence,
            boost::asio::yield_context &yield) const {
        auto hashes = fetchAllTransactionHashesInLedger(ledgerSequence, yield);
        return fetchTransactions(hashes, yield);
    }

    template<class Result>
    struct ReadCallbackData {
        using handler_type = typename Result::completion_handler_type;

        std::atomic_int &numOutstanding;
        handler_type handler;
        std::function<void(CassandraResult &)> onSuccess;

        std::atomic_bool errored = false;

        ReadCallbackData(
                std::atomic_int &numOutstanding,
                handler_type &handler,
                std::function<void(CassandraResult &)> onSuccess)
                : numOutstanding(numOutstanding), handler(handler), onSuccess(onSuccess) {
        }

        void
        finish(CassFuture *fut) {
            CassError rc = cass_future_error_code(fut);
            if (rc != CASS_OK) {
                errored = true;
            } else {
                CassandraResult result{cass_future_get_result(fut)};
                onSuccess(result);
            }

            if (--numOutstanding == 0)
                resume();
        }

        void
        resume() {
            boost::asio::post(
                    boost::asio::get_associated_executor(handler),
                    [handler = std::move(handler)]() mutable {
                        handler(boost::system::error_code{});
                    });
        }
    };

    void
    processAsyncRead(CassFuture *fut, void *cbData) {
        ReadCallbackData<result_type> &cb =
                *static_cast<ReadCallbackData<result_type> *>(cbData);
        cb.finish(fut);
    }

    std::vector<TransactionAndMetadata>
    CassandraBackend::fetchTransactions(
            std::vector<ripple::uint256> const &hashes,
            boost::asio::yield_context &yield) const {
        if (hashes.size() == 0)
            return {};

        handler_type handler(std::forward<decltype(yield)>(yield));
        result_type result(handler);

        std::size_t const numHashes = hashes.size();
        std::atomic_int numOutstanding = numHashes;
        std::vector<TransactionAndMetadata> results{numHashes};
        std::vector<std::shared_ptr<ReadCallbackData<result_type>>> cbs;
        cbs.reserve(numHashes);
        auto start = std::chrono::system_clock::now();

        for (std::size_t i = 0; i < hashes.size(); ++i) {
            CassandraStatement statement{selectTransaction_};
            statement.bindNextBytes(hashes[i]);

            cbs.push_back(std::make_shared<ReadCallbackData<result_type>>(
                    numOutstanding, handler, [i, &results](auto &result) {
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

        // suspend the coroutine until completion handler is called.
        result.get();

        auto end = std::chrono::system_clock::now();
        for (auto const &cb: cbs) {
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
            std::uint32_t const ledgerSequence,
            boost::asio::yield_context &yield) const {
        CassandraStatement statement{selectAllTransactionHashesInLedger_};
        statement.bindNextInt(ledgerSequence);
        auto start = std::chrono::system_clock::now();

        CassandraResult result = executeAsyncRead(statement, yield);

        auto end = std::chrono::system_clock::now();
        if (!result) {
            BOOST_LOG_TRIVIAL(error)
                << __func__
                << " - no rows . ledger = " << std::to_string(ledgerSequence);
            return {};
        }
        std::vector<ripple::uint256> hashes;
        do {
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
            ripple::AccountID const &account,
            std::uint32_t const limit,
            bool const forward,
            std::optional<AccountTransactionsCursor> const &cursorIn,
            boost::asio::yield_context &yield) const {
        auto rng = fetchLedgerRange();
        if (!rng)
            return {{},
                    {}};

        auto keylet = ripple::keylet::account(account);
        auto cursor = cursorIn;

        CassandraStatement statement = [this, forward]() {
            if (forward)
                return CassandraStatement{selectAccountTxForward_};
            else
                return CassandraStatement{selectAccountTx_};
        }();

        statement.bindNextBytes(account);
        if (cursor) {
            statement.bindNextIntTuple(
                    cursor->ledgerSequence, cursor->transactionIndex);
            BOOST_LOG_TRIVIAL(debug) << " account = " << ripple::strHex(account)
                                     << " tuple = " << cursor->ledgerSequence
                                     << " : " << cursor->transactionIndex;
        } else {
            int seq = forward ? rng->minSequence : rng->maxSequence;
            int placeHolder =
                    forward ? 0 : std::numeric_limits<std::uint32_t>::max();

            statement.bindNextIntTuple(placeHolder, placeHolder);
            BOOST_LOG_TRIVIAL(debug)
                << " account = " << ripple::strHex(account) << " idx = " << seq
                << " tuple = " << placeHolder;
        }
        statement.bindNextUInt(limit);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result.hasResult()) {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
            return {};
        }

        std::vector<ripple::uint256> hashes = {};
        auto numRows = result.numRows();
        BOOST_LOG_TRIVIAL(info) << "num_rows = " << std::to_string(numRows);
        do {
            hashes.push_back(result.getUInt256());
            if (--numRows == 0) {
                BOOST_LOG_TRIVIAL(debug) << __func__ << " setting cursor";
                auto[lgrSeq, txnIdx] = result.getInt64Tuple();
                cursor = {
                        static_cast<std::uint32_t>(lgrSeq),
                        static_cast<std::uint32_t>(txnIdx)};

                if (forward)
                    ++cursor->transactionIndex;
            }
        } while (result.nextRow());

        auto txns = fetchTransactions(hashes, yield);
        BOOST_LOG_TRIVIAL(debug) << __func__ << "txns = " << txns.size();

        if (txns.size() == limit) {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " returning cursor";
            return {txns, cursor};
        }

        return {txns, {}};
    }

    std::optional<ripple::uint256>
    CassandraBackend::doFetchSuccessorKey(
            ripple::uint256 key,
            std::uint32_t const ledgerSequence,
            boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{selectSuccessor_};
        statement.bindNextBytes(key);
        statement.bindNextInt(ledgerSequence);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result) {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows";
            return {};
        }
        auto next = result.getUInt256();
        if (next == lastKey)
            return {};
        return next;
    }

    std::optional<Blob>
    CassandraBackend::doFetchLedgerObject(
            ripple::uint256 const &key,
            std::uint32_t const sequence,
            boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{selectObject_};
        statement.bindNextBytes(key);
        statement.bindNextInt(sequence);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result) {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows";
            return {};
        }
        auto res = result.getBytes();
        if (res.size())
            return res;
        return {};
    }

    std::vector<Blob>
    CassandraBackend::doFetchLedgerObjects(
            std::vector<ripple::uint256> const &keys,
            std::uint32_t const sequence,
            boost::asio::yield_context &yield) const {
        if (keys.size() == 0)
            return {};

        handler_type handler(std::forward<decltype(yield)>(yield));
        result_type result(handler);

        std::size_t const numKeys = keys.size();
        BOOST_LOG_TRIVIAL(trace)
            << "Fetching " << numKeys << " records from Cassandra";
        std::atomic_int numOutstanding = numKeys;
        std::vector<Blob> results{numKeys};
        std::vector<std::shared_ptr<ReadCallbackData<result_type>>> cbs;
        cbs.reserve(numKeys);
        for (std::size_t i = 0; i < keys.size(); ++i) {
            cbs.push_back(std::make_shared<ReadCallbackData<result_type>>(
                    numOutstanding, handler, [i, &results](auto &result) {
                        if (result.hasResult())
                            results[i] = result.getBytes();
                    }));
            CassandraStatement statement{selectObject_};
            statement.bindNextBytes(keys[i]);
            statement.bindNextInt(sequence);
            executeAsyncRead(statement, processAsyncRead, *cbs[i]);
        }
        assert(results.size() == cbs.size());

        // suspend the coroutine until completion handler is called.
        result.get();

        for (auto const &cb: cbs) {
            if (cb->errored)
                throw DatabaseTimeout();
        }

        BOOST_LOG_TRIVIAL(trace)
            << "Fetched " << numKeys << " records from Cassandra";
        return results;
    }

    std::vector<LedgerObject>
    CassandraBackend::fetchLedgerDiff(
            std::uint32_t const ledgerSequence,
            boost::asio::yield_context &yield) const {
        CassandraStatement statement{selectDiff_};
        statement.bindNextInt(ledgerSequence);
        auto start = std::chrono::system_clock::now();

        CassandraResult result = executeAsyncRead(statement, yield);

        auto end = std::chrono::system_clock::now();

        if (!result) {
            BOOST_LOG_TRIVIAL(error)
                << __func__
                << " - no rows . ledger = " << std::to_string(ledgerSequence);
            return {};
        }
        std::vector<ripple::uint256> keys;
        do {
            keys.push_back(result.getUInt256());
        } while (result.nextRow());
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched " << keys.size() << " diff hashes from Cassandra in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                    .count()
            << " milliseconds";
        auto objs = fetchLedgerObjects(keys, ledgerSequence, yield);
        std::vector<LedgerObject> results;
        std::transform(
                keys.begin(),
                keys.end(),
                objs.begin(),
                std::back_inserter(results),
                [](auto const &k, auto const &o) {
                    return LedgerObject{k, o};
                });
        return results;
    }

    bool
    CassandraBackend::doOnlineDelete(
            std::uint32_t const numLedgersToKeep,
            boost::asio::yield_context &yield) const {
        // calculate TTL
        // ledgers close roughly every 4 seconds. We double the TTL so that way
        // there is a window of time to update the database, to prevent unchanging
        // records from being deleted.
        auto rng = fetchLedgerRange();
        if (!rng)
            return false;
        std::uint32_t minLedger = rng->maxSequence - numLedgersToKeep;
        if (minLedger <= rng->minSequence)
            return false;
        auto bind = [this](auto &params) {
            auto&[key, seq, obj] = params.data;
            CassandraStatement statement{insertObject_};
            statement.bindNextBytes(key);
            statement.bindNextInt(seq);
            statement.bindNextBytes(obj);
            return statement;
        };
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<std::shared_ptr<BulkWriteCallbackData<
                std::tuple<ripple::uint256, std::uint32_t, Blob>,
                typename std::remove_reference<decltype(bind)>::type>>>
                cbs;
        std::uint32_t concurrentLimit = 10;
        std::atomic_int numOutstanding = 0;

        // iterate through latest ledger, updating TTL
        std::optional<ripple::uint256> cursor;
        while (true) {
            auto[objects, curCursor] = retryOnTimeout(
                    [&]() { return fetchLedgerPage(cursor, minLedger, 256, yield); });

            for (auto &obj: objects) {
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
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
        CassandraStatement statement{deleteLedgerRange_};
        statement.bindNextInt(minLedger);
        executeSyncWrite(statement);
        // update ledger_range
        return true;
    }

    void
    CassandraBackend::open(bool readOnly) {
        auto getString = [this](std::string const &field) -> std::string {
            if (config_.contains(field)) {
                auto jsonStr = config_[field].as_string();
                return {jsonStr.c_str(), jsonStr.size()};
            }
            return {""};
        };
        auto getInt = [this](std::string const &field) -> std::optional<int> {
            if (config_.contains(field) && config_.at(field).is_int64())
                return config_[field].as_int64();
            return {};
        };
        if (open_) {
            assert(false);
            BOOST_LOG_TRIVIAL(error) << "database is already open";
            return;
        }

        BOOST_LOG_TRIVIAL(info) << "Opening Cassandra Backend";

        std::lock_guard<std::mutex> lock(mutex_);
        CassCluster *cluster = cass_cluster_new();
        if (!cluster)
            throw std::runtime_error("nodestore:: Failed to create CassCluster");

        std::string secureConnectBundle = getString("secure_connect_bundle");

        if (!secureConnectBundle.empty()) {
            /* Setup driver to connect to the cloud using the secure connection
             * bundle */
            if (cass_cluster_set_cloud_secure_connection_bundle(
                    cluster, secureConnectBundle.c_str()) != CASS_OK) {
                BOOST_LOG_TRIVIAL(error) << "Unable to configure cloud using the "
                                            "secure connection bundle: "
                                         << secureConnectBundle;
                throw std::runtime_error(
                        "nodestore: Failed to connect using secure connection "
                        "bundle");
                return;
            }
        } else {
            std::string contact_points = getString("contact_points");
            if (contact_points.empty()) {
                throw std::runtime_error(
                        "nodestore: Missing contact_points in Cassandra config");
            }
            CassError rc =
                    cass_cluster_set_contact_points(cluster, contact_points.c_str());
            if (rc != CASS_OK) {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra contact_points: "
                   << contact_points << ", result: " << rc << ", "
                   << cass_error_desc(rc);

                throw std::runtime_error(ss.str());
            }

            auto port = getInt("port");
            if (port) {
                rc = cass_cluster_set_port(cluster, *port);
                if (rc != CASS_OK) {
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
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "nodestore: Error setting cassandra protocol version: "
               << ", result: " << rc << ", " << cass_error_desc(rc);

            throw std::runtime_error(ss.str());
        }

        std::string username = getString("username");
        if (username.size()) {
            BOOST_LOG_TRIVIAL(debug) << "user = " << username.c_str();
            cass_cluster_set_credentials(
                    cluster, username.c_str(), getString("password").c_str());
        }
        int threads = getInt("threads") ? *getInt("threads")
                                        : std::thread::hardware_concurrency();

        rc = cass_cluster_set_num_threads_io(cluster, threads);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra io threads to " << threads
               << ", result: " << rc << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }
        if (getInt("max_requests_outstanding"))
            maxRequestsOutstanding = *getInt("max_requests_outstanding");

        if (getInt("sync_interval"))
            syncInterval_ = *getInt("sync_interval");
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " sync interval is " << syncInterval_
            << ". max requests outstanding is " << maxRequestsOutstanding;

        cass_cluster_set_request_timeout(cluster, 10000);

        rc = cass_cluster_set_queue_size_io(
                cluster,
                maxRequestsOutstanding);  // This number needs to scale w/ the
        // number of request per sec
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra max core connections per "
                  "host"
               << ", result: " << rc << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            throw std::runtime_error(ss.str());
        }

        std::string certfile = getString("certfile");
        if (certfile.size()) {
            std::ifstream fileStream(
                    boost::filesystem::path(certfile).string(), std::ios::in);
            if (!fileStream) {
                std::stringstream ss;
                ss << "opening config file " << certfile;
                throw std::system_error(errno, std::generic_category(), ss.str());
            }
            std::string cert(
                    std::istreambuf_iterator<char>{fileStream},
                    std::istreambuf_iterator<char>{});
            if (fileStream.bad()) {
                std::stringstream ss;
                ss << "reading config file " << certfile;
                throw std::system_error(errno, std::generic_category(), ss.str());
            }

            CassSsl *context = cass_ssl_new();
            cass_ssl_set_verify_flags(context, CASS_SSL_VERIFY_NONE);
            rc = cass_ssl_add_trusted_cert(context, cert.c_str());
            if (rc != CASS_OK) {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra ssl context: " << rc
                   << ", " << cass_error_desc(rc);
                throw std::runtime_error(ss.str());
            }

            cass_cluster_set_ssl(cluster, context);
            cass_ssl_free(context);
        }

        std::string keyspace = getString("keyspace");
        if (keyspace.empty()) {
            BOOST_LOG_TRIVIAL(warning)
                << "No keyspace specified. Using keyspace oceand";
            keyspace = "oceand";
        }

        int rf = getInt("replication_factor") ? *getInt("replication_factor") : 3;

        std::string tablePrefix = getString("table_prefix");
        if (tablePrefix.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "Table prefix is empty";
        }

        cass_cluster_set_connect_timeout(cluster, 10000);

        int ttl = getInt("ttl") ? *getInt("ttl") * 2 : 0;
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " setting ttl to " << std::to_string(ttl);

        auto executeSimpleStatement = [this](std::string const &query) {
            CassStatement *statement = makeStatement(query.c_str(), 0);
            CassFuture *fut = cass_session_execute(session_.get(), statement);
            CassError rc = cass_future_error_code(fut);
            cass_future_free(fut);
            cass_statement_free(statement);
            if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY) {
                std::stringstream ss;
                ss << "nodestore: Error executing simple statement: " << rc << ", "
                   << cass_error_desc(rc) << " - " << query;
                BOOST_LOG_TRIVIAL(error) << ss.str();
                return false;
            }
            return true;
        };
        CassFuture *fut;
        bool setupSessionAndTable = false;
        while (!setupSessionAndTable) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            session_.reset(cass_session_new());
            assert(session_);

            fut = cass_session_connect_keyspace(
                    session_.get(), cluster, keyspace.c_str());
            rc = cass_future_error_code(fut);
            cass_future_free(fut);
            if (rc != CASS_OK) {
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
                if (rc != CASS_OK) {
                    std::stringstream ss;
                    ss << "nodestore: Error connecting Cassandra session at all: "
                       << rc << ", " << cass_error_desc(rc);
                    BOOST_LOG_TRIVIAL(error) << ss.str();
                } else {
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
            query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "successor"
                  << " (key blob, seq bigint, next blob, PRIMARY KEY (key, seq)) "
                     " WITH default_time_to_live = "
                  << std::to_string(ttl);
            if (!executeSimpleStatement(query.str()))
                continue;

            query.str("");
            query << "SELECT * FROM " << tablePrefix << "successor"
                  << " LIMIT 1";
            if (!executeSimpleStatement(query.str()))
                continue;
            query.str("");
            query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "diff"
                  << " (seq bigint, key blob, PRIMARY KEY (seq, key)) "
                     " WITH default_time_to_live = "
                  << std::to_string(ttl);
            if (!executeSimpleStatement(query.str()))
                continue;

            query.str("");
            query << "SELECT * FROM " << tablePrefix << "diff"
                  << " LIMIT 1";
            if (!executeSimpleStatement(query.str()))
                continue;
            query.str("");
            query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "account_tx"
                  << " ( account blob, seq_idx "
                     "tuple<bigint, bigint>, "
                     " hash blob, "
                     "PRIMARY KEY "
                     "(account, seq_idx)) WITH "
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
        while (!setupPreparedStatements) {
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
            query << "INSERT INTO " << tablePrefix << "successor"
                  << " (key,seq,next) VALUES (?, ?, ?)";
            if (!insertSuccessor_.prepareStatement(query, session_.get()))
                continue;

            query.str("");
            query << "INSERT INTO " << tablePrefix << "diff"
                  << " (seq,key) VALUES (?, ?)";
            if (!insertDiff_.prepareStatement(query, session_.get()))
                continue;

            query.str("");
            query << "SELECT next FROM " << tablePrefix << "successor"
                  << " WHERE key = ? AND seq <= ? ORDER BY seq DESC LIMIT 1";
            if (!selectSuccessor_.prepareStatement(query, session_.get()))
                continue;

            query.str("");
            query << "SELECT key FROM " << tablePrefix << "diff"
                  << " WHERE seq = ?";
            if (!selectDiff_.prepareStatement(query, session_.get()))
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
                  << " (account, seq_idx, hash) "
                  << " VALUES (?,?,?)";
            if (!insertAccountTx_.prepareStatement(query, session_.get()))
                continue;

            query.str("");
            query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
                  << " WHERE account = ? "
                  << " AND seq_idx < ? LIMIT ?";
            if (!selectAccountTx_.prepareStatement(query, session_.get()))
                continue;
            query.str("");
            query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
                  << " WHERE account = ? "
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

        open_ = true;

        BOOST_LOG_TRIVIAL(info) << "Opened CassandraBackend successfully";
    }

    CassStatement *
    CassandraBackend::makeStatement(const char *query, std::size_t params) {
        CassStatement *ret = cass_statement_new(query, params);
        CassError rc =
                cass_statement_set_consistency(ret, CASS_CONSISTENCY_QUORUM);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "nodestore: Error setting query consistency: " << query
               << ", result: " << rc << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }
        return ret;
    }

    CassandraBackend::CassandraBackend(
            boost::asio::io_context &ioc,
            const boost::json::object &config)
            : BackendInterface(config), config_(config) {
        work_.emplace(ioContext_);
        ioThread_ = std::thread([this]() { ioContext_.run(); });
    }

    CassandraBackend::~CassandraBackend() {
        work_.reset();
        ioThread_.join();

        if (open_)
            close();
    }

    boost::asio::io_context &
    CassandraBackend::getIOContext() const {
        return ioContext_;
    }

    bool
    CassandraBackend::isOpen() {
        return open_;
    }


    std::optional<std::uint32_t>
    CassandraBackend::fetchLatestLedgerSequence(
            boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectLatestLedger_};
        CassandraResult result = executeAsyncRead(statement, yield);
        if (!result.hasResult()) {
            BOOST_LOG_TRIVIAL(error)
                << "CassandraBackend::fetchLatestLedgerSequence - no rows";
            return {};
        }
        return result.getUInt32();
    }

    std::optional<ripple::LedgerInfo>
    CassandraBackend::fetchLedgerBySequence(
            const std::uint32_t sequence,
            boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectLedgerBySeq_};
        statement.bindNextInt(sequence);
        CassandraResult result = executeAsyncRead(statement, yield);
        if (!result) {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        std::vector<unsigned char> header = result.getBytes();
        return deserializeHeader(ripple::makeSlice(header));
    }

    std::optional<ripple::LedgerInfo>
    CassandraBackend::fetchLedgerByHash(
            const ripple::uint256 &hash,
            boost::asio::yield_context &yield) const {
        CassandraStatement statement{selectLedgerByHash_};

        statement.bindNextBytes(hash);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result.hasResult()) {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
            return {};
        }

        std::uint32_t const sequence = result.getInt64();

        return fetchLedgerBySequence(sequence, yield);
    }

    std::optional<int64_t>
    CassandraBackend::getToken(const void *key, boost::asio::yield_context &yield)
    const {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{getToken_};
        statement.bindNextBytes(key, 32);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result) {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        int64_t token = result.getInt64();
        if (token == INT64_MAX)
            return {};
        else
            return token + 1;
    }

    std::optional<TransactionAndMetadata>
    CassandraBackend::fetchTransaction(
            const ripple::uint256 &hash,
            boost::asio::yield_context &yield) const {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectTransaction_};
        statement.bindNextBytes(hash);
        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result) {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        return {
                {result.getBytes(),
                 result.getBytes(),
                 result.getUInt32(),
                 result.getUInt32()}};
    }

    void
    CassandraBackend::startWrites() const {
    }

    void
    CassandraBackend::sync() const {
        std::unique_lock<std::mutex> lck(syncMutex_);

        syncCv_.wait(lck, [this]() { return finishedAllRequests(); });
    }

    void
    CassandraBackend::decrementOutstandingRequestCount() const {
        // sanity check
        if (numRequestsOutstanding_ == 0) {
            assert(false);
            throw std::runtime_error("decrementing num outstanding below 0");
        }
        size_t cur = (--numRequestsOutstanding_);
        {
            // mutex lock required to prevent race condition around spurious
            // wakeup
            std::lock_guard lck(throttleMutex_);
            throttleCv_.notify_one();
        }
        if (cur == 0) {
            // mutex lock required to prevent race condition around spurious
            // wakeup
            std::lock_guard lck(syncMutex_);
            syncCv_.notify_one();
        }
    }

    bool
    CassandraBackend::canAddRequest() const {
        return numRequestsOutstanding_ < maxRequestsOutstanding;
    }

    void
    CassandraBackend::finishAsyncWrite() const {
        decrementOutstandingRequestCount();
    }

    bool
    CassandraBackend::finishedAllRequests() const {
        return numRequestsOutstanding_ == 0;
    }

// TODO: need more testing for the following three methods
    template<class T, class S>
    void
    CassandraBackend::executeAsyncHelper(
            const CassandraStatement &statement,
            T callback,
            S &callbackData) const {
        CassFuture *fut = cass_session_execute(session_.get(), statement.get());

        cass_future_set_callback(
                fut, callback, static_cast<void *>(&callbackData));

        cass_future_free(fut);
    }

    template<class T, class S>
    void
    CassandraBackend::executeAsyncWrite(
            const CassandraStatement &statement,
            T callback,
            S &callbackData,
            bool isRetry) const {
        if (!isRetry)
            incrementOutstandingRequestCount();
        executeAsyncHelper(statement, callback, callbackData);
    }

    template<class T, class S>
    void
    CassandraBackend::executeAsyncRead(
            const CassandraStatement &statement,
            T callback,
            S &callbackData) const {
        executeAsyncHelper(statement, callback, callbackData);
    }

    void
    CassandraBackend::executeSyncWrite(const CassandraStatement &statement) const {
        CassFuture *fut;
        CassError rc;
        do {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK) {
                std::stringstream ss;
                ss << "Cassandra sync write error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } while (rc != CASS_OK);
        cass_future_free(fut);
    }

    bool
    CassandraBackend::executeSyncUpdate(const CassandraStatement &statement) const {
        bool timedOut = false;
        CassFuture *fut;
        CassError rc;
        do {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK) {
                timedOut = true;
                std::stringstream ss;
                ss << "Cassandra sync update error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } while (rc != CASS_OK);
        CassResult const *res = cass_future_get_result(fut);
        cass_future_free(fut);

        CassRow const *row = cass_result_first_row(res);
        if (!row) {
            BOOST_LOG_TRIVIAL(error) << "executeSyncUpdate - no rows";
            cass_result_free(res);
            return false;
        }
        cass_bool_t success;
        rc = cass_value_get_bool(cass_row_get_column(row, 0), &success);
        if (rc != CASS_OK) {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "executeSyncUpdate - error getting result " << rc << ", "
                << cass_error_desc(rc);
            return false;
        }
        cass_result_free(res);
        if (success != cass_true && timedOut) {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " Update failed, but timedOut is true";
        }
        // if there was a timeout, the update may have succeeded in the
        // background. We can't differentiate between an async success and
        // another writer, so we just return true here
        return success == cass_true || timedOut;
    }

    CassandraResult
    CassandraBackend::executeAsyncRead(
            const CassandraStatement &statement,
            boost::asio::yield_context &yield) const {
        using result = boost::asio::async_result<
                boost::asio::yield_context,
                void(boost::system::error_code, CassError)>;

        CassFuture *fut;
        CassError rc;
        do {
            fut = cass_session_execute(session_.get(), statement.get());

            boost::system::error_code ec;
            rc = cass_future_error_code(fut, yield[ec]);

            if (ec) {
                BOOST_LOG_TRIVIAL(error)
                    << "Cannot read async cass_future_error_code";
            }
            if (rc != CASS_OK) {
                std::stringstream ss;
                ss << "Cassandra executeAsyncRead error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
            }
            if (isTimeout(rc)) {
                cass_future_free(fut);
                throw DatabaseTimeout();
            }

            if (rc == CASS_ERROR_SERVER_INVALID_QUERY) {
                throw std::runtime_error("invalid query");
            }
        } while (rc != CASS_OK);

        // The future should have returned at the earlier cass_future_error_code
        // so we can use the sync version of this function.
        CassResult const *res = cass_future_get_result(fut);
        cass_future_free(fut);
        return {res};
    }

    void
    CassandraBackend::incrementOutstandingRequestCount() const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!canAddRequest())
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() { return canAddRequest(); });
            }
        }
        ++numRequestsOutstanding_;
    }

    bool CassandraBackend::doFinishWrites() {
        // if db is empty, sync. if sync interval is 1, always sync.
        // if we've never synced, sync. if its been greater than the configured
        // sync interval since we last synced, sync.
        if (!range || syncInterval_ == 1 || lastSync_ == 0 ||
            ledgerSequence_ - syncInterval_ >= lastSync_) {
            // wait for all other writes to finish
            sync();
            // write range
            if (!range) {
                CassandraStatement statement{updateLedgerRange_};
                statement.bindNextInt(ledgerSequence_);
                statement.bindNextBoolean(false);
                statement.bindNextInt(ledgerSequence_);
                executeSyncWrite(statement);
            }
            CassandraStatement statement{updateLedgerRange_};
            statement.bindNextInt(ledgerSequence_);
            statement.bindNextBoolean(true);
            if (lastSync_ == 0)
                statement.bindNextInt(ledgerSequence_ - 1);
            else
                statement.bindNextInt(lastSync_);
            if (!executeSyncUpdate(statement)) {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " Update failed for ledger "
                    << std::to_string(ledgerSequence_) << ". Returning";
                return false;
            }
            BOOST_LOG_TRIVIAL(info) << __func__ << " Committed ledger "
                                    << std::to_string(ledgerSequence_);
            lastSync_ = ledgerSequence_;
        } else {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Skipping commit. sync interval is "
                << std::to_string(syncInterval_) << " - last sync is "
                << std::to_string(lastSync_) << " - ledger sequence is "
                << std::to_string(ledgerSequence_);
        }
        return true;
    }

    CassPrepared const *
    CassandraPreparedStatement::get() const {
        return prepared_;
    }

    bool
    CassandraPreparedStatement::prepareStatement(
            const std::stringstream &query,
            CassSession *session) {
        return prepareStatement(query.str().c_str(), session);
    }

    bool
    CassandraPreparedStatement::prepareStatement(
            const std::string &query,
            CassSession *session) {
        return prepareStatement(query.c_str(), session);
    }

    bool
    CassandraPreparedStatement::prepareStatement(
            const char *query,
            CassSession *session) {
        if (!query)
            throw std::runtime_error("prepareStatement: null query");
        if (!session)
            throw std::runtime_error("prepareStatement: null sesssion");
        CassFuture *prepareFuture = cass_session_prepare(session, query);
        /* Wait for the statement to prepare and get the result */
        CassError rc = cass_future_error_code(prepareFuture);
        if (rc == CASS_OK) {
            prepared_ = cass_future_get_prepared(prepareFuture);
        } else {
            std::stringstream ss;
            ss << "nodestore: Error preparing statement : " << rc << ", "
               << cass_error_desc(rc) << ". query : " << query;
            BOOST_LOG_TRIVIAL(error) << ss.str();
        }
        cass_future_free(prepareFuture);
        return rc == CASS_OK;
    }

    CassandraPreparedStatement::~CassandraPreparedStatement() {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        if (prepared_) {
            cass_prepared_free(prepared_);
            prepared_ = nullptr;
        }
    }

    CassandraStatement::CassandraStatement(
            const CassandraPreparedStatement &prepared) {
        statement_ = cass_prepared_bind(prepared.get());
        cass_statement_set_consistency(statement_, CASS_CONSISTENCY_QUORUM);
    }

    CassandraStatement::CassandraStatement(CassandraStatement &&other) {
        statement_ = other.statement_;
        other.statement_ = nullptr;
        curBindingIndex_ = other.curBindingIndex_;
        other.curBindingIndex_ = 0;
    }

    CassStatement *
    CassandraStatement::get() const {
        return statement_;
    }

    void
    CassandraStatement::bindNextBoolean(bool val) {
        if (!statement_)
            throw std::runtime_error(
                    "CassandraStatement::bindNextBoolean - statement_ is null");
        CassError rc = cass_statement_bind_bool(
                statement_, 1, static_cast<cass_bool_t>(val));
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding boolean to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    CassandraStatement::bindNextBytes(const char *data, const std::uint32_t size) {
        bindNextBytes((unsigned const char *) (data), size);
    }

    void
    CassandraStatement::bindNextBytes(const ripple::uint256 &data) {
        bindNextBytes(data.data(), data.size());
    }

    void
    CassandraStatement::bindNextBytes(const std::vector<unsigned char> &data) {
        bindNextBytes(data.data(), data.size());
    }

    void
    CassandraStatement::bindNextBytes(const ripple::AccountID &data) {
        bindNextBytes(data.data(), data.size());
    }

    void
    CassandraStatement::bindNextBytes(const std::string &data) {
        bindNextBytes(data.data(), data.size());
    }

    void
    CassandraStatement::bindNextBytes(
            const unsigned char *data,
            const std::uint32_t size) {
        if (!statement_)
            throw std::runtime_error(
                    "CassandraStatement::bindNextBytes - statement_ is null");
        CassError rc = cass_statement_bind_bytes(
                statement_,
                curBindingIndex_,
                static_cast<cass_byte_t const *>(data),
                size);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding bytes to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    CassandraStatement::bindNextBytes(const void *key, const std::uint32_t size) {
        bindNextBytes(static_cast<const unsigned char *>(key), size);
    }

    void
    CassandraStatement::bindNextUInt(const std::uint32_t value) {
        if (!statement_)
            throw std::runtime_error(
                    "CassandraStatement::bindNextUInt - statement_ is null");
        BOOST_LOG_TRIVIAL(trace)
            << std::to_string(curBindingIndex_)
            << " " << std::to_string(value);
        CassError rc =
                cass_statement_bind_int32(statement_,
                                          curBindingIndex_, value);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding uint to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    CassandraStatement::bindNextInt(const std::uint32_t value) {
        bindNextInt(static_cast<std::int64_t>(value));
    }

    void
    CassandraStatement::bindNextInt(int64_t value) {
        if (!statement_)
            throw std::runtime_error(
                    "CassandraStatement::bindNextInt - statement_ is null");
        CassError rc =
                cass_statement_bind_int64(statement_, curBindingIndex_, value);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding int to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    CassandraStatement::bindNextIntTuple(
            const std::uint32_t first,
            const std::uint32_t second) {
        CassTuple *tuple = cass_tuple_new(2);
        CassError rc = cass_tuple_set_int64(tuple, 0, first);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_tuple_set_int64(tuple, 1, second);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_tuple(statement_,
                                       curBindingIndex_, tuple);
        if (rc != CASS_OK) {
            std::stringstream ss;
            ss << "Error binding tuple to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        cass_tuple_free(tuple);
        curBindingIndex_++;
    }

    CassandraStatement::~CassandraStatement() {
        if (statement_)
            cass_statement_free(statement_);
    }

    CassandraResult::CassandraResult(const CassResult *result) : result_(result) {
        if (!result_)
            throw std::runtime_error("CassandraResult - result is null");
        iter_ = cass_iterator_from_result(result_);
        if (cass_iterator_next(iter_)) {
            row_ = cass_iterator_get_row(iter_);
        }
    }

    bool
    CassandraResult::isOk() {
        return result_ != nullptr;
    }

    bool
    CassandraResult::hasResult() {
        return row_ != nullptr;
    }

    bool
    CassandraResult::operator!() {
        return !hasResult();
    }

    size_t
    CassandraResult::numRows() {
        return cass_result_row_count(result_);
    }

    bool
    CassandraResult::nextRow() {
        curGetIndex_ = 0;
        if (cass_iterator_next(iter_)) {
            row_ = cass_iterator_get_row(iter_);
            return true;
        }
        row_ = nullptr;
        return false;
    }

    std::vector<unsigned char>
    CassandraResult::getBytes() {
        if (!row_)
            throw std::runtime_error("CassandraResult::getBytes - no result");
        cass_byte_t const *buf;
        std::size_t bufSize;
        CassError rc = cass_value_get_bytes(
                cass_row_get_column(row_, curGetIndex_), &buf, &bufSize);
        if (rc != CASS_OK) {
            std::stringstream msg;
            msg << "CassandraResult::getBytes - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        curGetIndex_++;
        return {buf, buf + bufSize};
    }

    ripple::uint256
    CassandraResult::getUInt256() {
        if (!row_)
            throw std::runtime_error("CassandraResult::uint256 - no result");
        cass_byte_t const *buf;
        std::size_t bufSize;
        CassError rc = cass_value_get_bytes(
                cass_row_get_column(row_, curGetIndex_), &buf, &bufSize);
        if (rc != CASS_OK) {
            std::stringstream msg;
            msg << "CassandraResult::getuint256 - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        curGetIndex_++;
        return ripple::uint256::fromVoid(buf);
    }

    int64_t
    CassandraResult::getInt64() {
        if (!row_)
            throw std::runtime_error("CassandraResult::getInt64 - no result");
        cass_int64_t val;
        CassError rc =
                cass_value_get_int64(cass_row_get_column(row_, curGetIndex_), &val);
        if (rc != CASS_OK) {
            std::stringstream msg;
            msg << "CassandraResult::getInt64 - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        ++curGetIndex_;
        return val;
    }

    std::uint32_t
    CassandraResult::getUInt32() {
        return static_cast<std::uint32_t>(getInt64());
    }

    std::pair<std::int64_t, std::int64_t>
    CassandraResult::getInt64Tuple() {
        if (!row_)
            throw std::runtime_error(
                    "CassandraResult::getInt64Tuple - no result");

        CassValue const *tuple = cass_row_get_column(row_, curGetIndex_);
        CassIterator *tupleIter = cass_iterator_from_tuple(tuple);

        if (!cass_iterator_next(tupleIter)) {
            cass_iterator_free(tupleIter);
            throw std::runtime_error(
                    "CassandraResult::getInt64Tuple - failed to iterate tuple");
        }

        CassValue const *value = cass_iterator_get_value(tupleIter);
        std::int64_t first;
        cass_value_get_int64(value, &first);
        if (!cass_iterator_next(tupleIter)) {
            cass_iterator_free(tupleIter);
            throw std::runtime_error(
                    "CassandraResult::getInt64Tuple - failed to iterate tuple");
        }

        value = cass_iterator_get_value(tupleIter);
        std::int64_t second;
        cass_value_get_int64(value, &second);
        cass_iterator_free(tupleIter);

        ++curGetIndex_;
        return {first, second};
    }

    std::pair<Blob, Blob>
    CassandraResult::getBytesTuple() {
        cass_byte_t const *buf;
        std::size_t bufSize;

        if (!row_)
            throw std::runtime_error(
                    "CassandraResult::getBytesTuple - no result");
        CassValue const *tuple = cass_row_get_column(row_, curGetIndex_);
        CassIterator *tupleIter = cass_iterator_from_tuple(tuple);
        if (!cass_iterator_next(tupleIter))
            throw std::runtime_error(
                    "CassandraResult::getBytesTuple - failed to iterate tuple");
        CassValue const *value = cass_iterator_get_value(tupleIter);
        cass_value_get_bytes(value, &buf, &bufSize);
        Blob first{buf, buf + bufSize};

        if (!cass_iterator_next(tupleIter))
            throw std::runtime_error(
                    "CassandraResult::getBytesTuple - failed to iterate tuple");
        value = cass_iterator_get_value(tupleIter);
        cass_value_get_bytes(value, &buf, &bufSize);
        Blob second{buf, buf + bufSize};
        ++curGetIndex_;
        return {first, second};
    }

    CassandraResult::~CassandraResult() {
        if (result_ != nullptr)
            cass_result_free(result_);
        if (iter_ != nullptr)
            cass_iterator_free(iter_);
    }

    bool
    isTimeout(CassError rc) {
        if (rc == CASS_ERROR_LIB_NO_HOSTS_AVAILABLE or
            rc == CASS_ERROR_LIB_REQUEST_TIMED_OUT or
            rc == CASS_ERROR_SERVER_UNAVAILABLE or
            rc == CASS_ERROR_SERVER_OVERLOADED or
            rc == CASS_ERROR_SERVER_READ_TIMEOUT)
            return true;
        return false;
    }
}  // namespace Backend
