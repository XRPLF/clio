#include <ripple/app/tx/impl/details/NFTokenUtils.h>
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
            << cass_error_desc(rc) << " id= " << requestParams.toString()
            << ", current retries " << requestParams.currentRetries
            << ", retrying in " << wait.count() << " milliseconds";
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
    std::uint32_t currentRetries;
    std::atomic<int> refs = 1;
    std::string id;

    WriteCallbackData(
        CassandraBackend const* b,
        T&& d,
        B bind,
        std::string const& identifier)
        : backend(b), data(std::move(d)), id(identifier)
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

    std::string
    toString()
    {
        return id;
    }
};

template <class T, class B>
struct BulkWriteCallbackData : public WriteCallbackData<T, B>
{
    std::atomic_int& numRemaining;
    std::mutex& mtx;
    std::condition_variable& cv;
    BulkWriteCallbackData(
        CassandraBackend const* b,
        T&& d,
        B bind,
        std::atomic_int& r,
        std::mutex& m,
        std::condition_variable& c)
        : WriteCallbackData<T, B>(b, std::move(d), bind, "bulk")
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
makeAndExecuteAsyncWrite(
    CassandraBackend const* b,
    T&& d,
    B bind,
    std::string const& id)
{
    auto* cb = new WriteCallbackData<T, B>(b, std::move(d), bind, id);
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
    std::uint32_t const seq,
    std::string&& blob)
{
    BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
    if (range)
        makeAndExecuteAsyncWrite(
            this,
            std::make_tuple(seq, key),
            [this](auto& params) {
                auto& [sequence, key] = params.data;

                CassandraStatement statement{insertDiff_};
                statement.bindNextInt(sequence);
                statement.bindNextBytes(key);
                return statement;
            },
            "ledger_diff");
    makeAndExecuteAsyncWrite(
        this,
        std::make_tuple(std::move(key), seq, std::move(blob)),
        [this](auto& params) {
            auto& [key, sequence, blob] = params.data;

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
    std::string&& key,
    std::uint32_t const seq,
    std::string&& successor)
{
    BOOST_LOG_TRIVIAL(trace)
        << "Writing successor. key = " << key
        << " seq = " << std::to_string(seq) << " successor = " << successor;
    assert(key.size() != 0);
    assert(successor.size() != 0);
    makeAndExecuteAsyncWrite(
        this,
        std::make_tuple(std::move(key), seq, std::move(successor)),
        [this](auto& params) {
            auto& [key, sequence, successor] = params.data;

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
    ripple::LedgerInfo const& ledgerInfo,
    std::string&& header)
{
    makeAndExecuteAsyncWrite(
        this,
        std::make_tuple(ledgerInfo.seq, std::move(header)),
        [this](auto& params) {
            auto& [sequence, header] = params.data;
            CassandraStatement statement{insertLedgerHeader_};
            statement.bindNextInt(sequence);
            statement.bindNextBytes(header);
            return statement;
        },
        "ledger");
    makeAndExecuteAsyncWrite(
        this,
        std::make_tuple(ledgerInfo.hash, ledgerInfo.seq),
        [this](auto& params) {
            auto& [hash, sequence] = params.data;
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
    std::vector<AccountTransactionsData>&& data)
{
    for (auto& record : data)
    {
        for (auto& account : record.accounts)
        {
            makeAndExecuteAsyncWrite(
                this,
                std::make_tuple(
                    std::move(account),
                    record.ledgerSequence,
                    record.transactionIndex,
                    record.txHash),
                [this](auto& params) {
                    CassandraStatement statement(insertAccountTx_);
                    auto& [account, lgrSeq, txnIdx, hash] = params.data;
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
CassandraBackend::writeNFTTransactions(std::vector<NFTTransactionsData>&& data)
{
    for (NFTTransactionsData const& record : data)
    {
        makeAndExecuteAsyncWrite(
            this,
            std::make_tuple(
                record.tokenID,
                record.ledgerSequence,
                record.transactionIndex,
                record.txHash),
            [this](auto const& params) {
                CassandraStatement statement(insertNFTTx_);
                auto const& [tokenID, lgrSeq, txnIdx, txHash] = params.data;
                statement.bindNextBytes(tokenID);
                statement.bindNextIntTuple(lgrSeq, txnIdx);
                statement.bindNextBytes(txHash);
                return statement;
            },
            "nf_token_transactions");
    }
}

void
CassandraBackend::writeTransaction(
    std::string&& hash,
    std::uint32_t const seq,
    std::uint32_t const date,
    std::string&& transaction,
    std::string&& metadata)
{
    BOOST_LOG_TRIVIAL(trace) << "Writing txn to cassandra";
    std::string hashCpy = hash;

    makeAndExecuteAsyncWrite(
        this,
        std::make_pair(seq, hash),
        [this](auto& params) {
            CassandraStatement statement{insertLedgerTransaction_};
            statement.bindNextInt(params.data.first);
            statement.bindNextBytes(params.data.second);
            return statement;
        },
        "ledger_transaction");
    makeAndExecuteAsyncWrite(
        this,
        std::make_tuple(
            std::move(hash),
            seq,
            date,
            std::move(transaction),
            std::move(metadata)),
        [this](auto& params) {
            CassandraStatement statement{insertTransaction_};
            auto& [hash, sequence, date, transaction, metadata] = params.data;
            statement.bindNextBytes(hash);
            statement.bindNextInt(sequence);
            statement.bindNextInt(date);
            statement.bindNextBytes(transaction);
            statement.bindNextBytes(metadata);
            return statement;
        },
        "transaction");
}

void
CassandraBackend::writeNFTs(std::vector<NFTsData>&& data)
{
    for (NFTsData const& record : data)
    {
        makeAndExecuteAsyncWrite(
            this,
            std::make_tuple(
                record.tokenID,
                record.ledgerSequence,
                record.owner,
                record.isBurned),
            [this](auto const& params) {
                CassandraStatement statement{insertNFT_};
                auto const& [tokenID, lgrSeq, owner, isBurned] = params.data;
                statement.bindNextBytes(tokenID);
                statement.bindNextInt(lgrSeq);
                statement.bindNextBytes(owner);
                statement.bindNextBoolean(isBurned);
                return statement;
            },
            "nf_tokens");

        makeAndExecuteAsyncWrite(
            this,
            std::make_tuple(record.tokenID),
            [this](auto const& params) {
                CassandraStatement statement{insertIssuerNFT_};
                auto const& [tokenID] = params.data;
                statement.bindNextBytes(ripple::nft::getIssuer(tokenID));
                statement.bindNextBytes(tokenID);
                return statement;
            },
            "issuer_nf_tokens");
    }
}

std::optional<LedgerRange>
CassandraBackend::hardFetchLedgerRange(boost::asio::yield_context& yield) const
{
    BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
    CassandraStatement statement{selectLedgerRange_};
    CassandraResult result = executeAsyncRead(statement, yield);

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
CassandraBackend::fetchAllTransactionsInLedger(
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    auto hashes = fetchAllTransactionHashesInLedger(ledgerSequence, yield);
    return fetchTransactions(hashes, yield);
}

template <class Result>
struct ReadCallbackData
{
    using handler_type = typename Result::completion_handler_type;

    std::atomic_int& numOutstanding;
    handler_type handler;
    std::function<void(CassandraResult&)> onSuccess;

    std::atomic_bool errored = false;
    ReadCallbackData(
        std::atomic_int& numOutstanding,
        handler_type& handler,
        std::function<void(CassandraResult&)> onSuccess)
        : numOutstanding(numOutstanding), handler(handler), onSuccess(onSuccess)
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

        if (--numOutstanding == 0)
            resume();
    }

    void
    resume()
    {
        boost::asio::post(
            boost::asio::get_associated_executor(handler),
            [handler = std::move(handler)]() mutable {
                handler(boost::system::error_code{});
            });
    }
};

void
processAsyncRead(CassFuture* fut, void* cbData)
{
    ReadCallbackData<result_type>& cb =
        *static_cast<ReadCallbackData<result_type>*>(cbData);
    cb.finish(fut);
}

std::vector<TransactionAndMetadata>
CassandraBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes,
    boost::asio::yield_context& yield) const
{
    if (hashes.size() == 0)
        return {};
    numReadRequestsOutstanding_ += hashes.size();

    handler_type handler(std::forward<decltype(yield)>(yield));
    result_type result(handler);

    std::size_t const numHashes = hashes.size();
    std::atomic_int numOutstanding = numHashes;
    std::vector<TransactionAndMetadata> results{numHashes};
    std::vector<std::shared_ptr<ReadCallbackData<result_type>>> cbs;
    cbs.reserve(numHashes);
    auto start = std::chrono::system_clock::now();

    for (std::size_t i = 0; i < hashes.size(); ++i)
    {
        CassandraStatement statement{selectTransaction_};
        statement.bindNextBytes(hashes[i]);

        cbs.push_back(std::make_shared<ReadCallbackData<result_type>>(
            numOutstanding, handler, [i, &results](auto& result) {
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
    numReadRequestsOutstanding_ -= hashes.size();

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
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    CassandraStatement statement{selectAllTransactionHashesInLedger_};
    statement.bindNextInt(ledgerSequence);
    auto start = std::chrono::system_clock::now();

    CassandraResult result = executeAsyncRead(statement, yield);

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

std::optional<NFT>
CassandraBackend::fetchNFT(
    ripple::uint256 const& tokenID,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    CassandraStatement statement{selectNFT_};
    statement.bindNextBytes(tokenID);
    statement.bindNextInt(ledgerSequence);
    CassandraResult response = executeAsyncRead(statement, yield);
    if (!response)
        return {};

    NFT result;
    result.tokenID = tokenID;
    result.ledgerSequence = response.getUInt32();
    result.owner = response.getBytes();
    result.isBurned = response.getBool();
    return result;
}

TransactionsAndCursor
CassandraBackend::fetchNFTTransactions(
    ripple::uint256 const& tokenID,
    std::uint32_t const limit,
    bool const forward,
    std::optional<TransactionsCursor> const& cursorIn,
    boost::asio::yield_context& yield) const
{
    auto cursor = cursorIn;
    auto rng = fetchLedgerRange();
    if (!rng)
        return {{}, {}};

    CassandraStatement statement = forward
        ? CassandraStatement(selectNFTTxForward_)
        : CassandraStatement(selectNFTTx_);

    statement.bindNextBytes(tokenID);

    if (cursor)
    {
        statement.bindNextIntTuple(
            cursor->ledgerSequence, cursor->transactionIndex);
        BOOST_LOG_TRIVIAL(debug) << " token_id = " << ripple::strHex(tokenID)
                                 << " tuple = " << cursor->ledgerSequence
                                 << " : " << cursor->transactionIndex;
    }
    else
    {
        int const seq = forward ? rng->minSequence : rng->maxSequence;
        int const placeHolder =
            forward ? 0 : std::numeric_limits<std::uint32_t>::max();

        statement.bindNextIntTuple(placeHolder, placeHolder);
        BOOST_LOG_TRIVIAL(debug)
            << " token_id = " << ripple::strHex(tokenID) << " idx = " << seq
            << " tuple = " << placeHolder;
    }

    statement.bindNextUInt(limit);

    CassandraResult result = executeAsyncRead(statement, yield);

    if (!result.hasResult())
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
        return {};
    }

    std::vector<ripple::uint256> hashes = {};
    auto numRows = result.numRows();
    BOOST_LOG_TRIVIAL(info) << "num_rows = " << numRows;
    do
    {
        hashes.push_back(result.getUInt256());
        if (--numRows == 0)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " setting cursor";
            auto const [lgrSeq, txnIdx] = result.getInt64Tuple();
            cursor = {
                static_cast<std::uint32_t>(lgrSeq),
                static_cast<std::uint32_t>(txnIdx)};

            // Only modify if forward because forward query
            // (selectNFTTxForward_) orders by ledger/tx sequence >= whereas
            // reverse query (selectNFTTx_) orders by ledger/tx sequence <.
            if (forward)
                ++cursor->transactionIndex;
        }
    } while (result.nextRow());

    auto txns = fetchTransactions(hashes, yield);
    BOOST_LOG_TRIVIAL(debug) << __func__ << " txns = " << txns.size();

    if (txns.size() == limit)
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " returning cursor";
        return {txns, cursor};
    }

    return {txns, {}};
}

TransactionsAndCursor
CassandraBackend::fetchAccountTransactions(
    ripple::AccountID const& account,
    std::uint32_t const limit,
    bool const forward,
    std::optional<TransactionsCursor> const& cursorIn,
    boost::asio::yield_context& yield) const
{
    auto rng = fetchLedgerRange();
    if (!rng)
        return {{}, {}};

    CassandraStatement statement = [this, forward]() {
        if (forward)
            return CassandraStatement{selectAccountTxForward_};
        else
            return CassandraStatement{selectAccountTx_};
    }();

    auto cursor = cursorIn;
    statement.bindNextBytes(account);
    if (cursor)
    {
        statement.bindNextIntTuple(
            cursor->ledgerSequence, cursor->transactionIndex);
        BOOST_LOG_TRIVIAL(debug) << " account = " << ripple::strHex(account)
                                 << " tuple = " << cursor->ledgerSequence
                                 << " : " << cursor->transactionIndex;
    }
    else
    {
        int const seq = forward ? rng->minSequence : rng->maxSequence;
        int const placeHolder =
            forward ? 0 : std::numeric_limits<std::uint32_t>::max();

        statement.bindNextIntTuple(placeHolder, placeHolder);
        BOOST_LOG_TRIVIAL(debug)
            << " account = " << ripple::strHex(account) << " idx = " << seq
            << " tuple = " << placeHolder;
    }
    statement.bindNextUInt(limit);

    CassandraResult result = executeAsyncRead(statement, yield);

    if (!result.hasResult())
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
        return {};
    }

    std::vector<ripple::uint256> hashes = {};
    auto numRows = result.numRows();
    BOOST_LOG_TRIVIAL(info) << "num_rows = " << std::to_string(numRows);
    do
    {
        hashes.push_back(result.getUInt256());
        if (--numRows == 0)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " setting cursor";
            auto [lgrSeq, txnIdx] = result.getInt64Tuple();
            cursor = {
                static_cast<std::uint32_t>(lgrSeq),
                static_cast<std::uint32_t>(txnIdx)};

            // Only modify if forward because forward query
            // (selectAccountTxForward_) orders by ledger/tx sequence >= whereas
            // reverse query (selectAccountTx_) orders by ledger/tx sequence <.
            if (forward)
                ++cursor->transactionIndex;
        }
    } while (result.nextRow());

    auto txns = fetchTransactions(hashes, yield);
    BOOST_LOG_TRIVIAL(debug) << __func__ << "txns = " << txns.size();

    if (txns.size() == limit)
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " returning cursor";
        return {txns, cursor};
    }

    return {txns, {}};
}

std::optional<ripple::uint256>
CassandraBackend::doFetchSuccessorKey(
    ripple::uint256 key,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
    CassandraStatement statement{selectSuccessor_};
    statement.bindNextBytes(key);
    statement.bindNextInt(ledgerSequence);

    CassandraResult result = executeAsyncRead(statement, yield);

    if (!result)
    {
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
    ripple::uint256 const& key,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
    CassandraStatement statement{selectObject_};
    statement.bindNextBytes(key);
    statement.bindNextInt(sequence);

    CassandraResult result = executeAsyncRead(statement, yield);

    if (!result)
    {
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
    std::vector<ripple::uint256> const& keys,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    if (keys.size() == 0)
        return {};

    numReadRequestsOutstanding_ += keys.size();

    handler_type handler(std::forward<decltype(yield)>(yield));
    result_type result(handler);

    std::size_t const numKeys = keys.size();
    BOOST_LOG_TRIVIAL(trace)
        << "Fetching " << numKeys << " records from Cassandra";
    std::atomic_int numOutstanding = numKeys;
    std::vector<Blob> results{numKeys};
    std::vector<std::shared_ptr<ReadCallbackData<result_type>>> cbs;
    cbs.reserve(numKeys);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        cbs.push_back(std::make_shared<ReadCallbackData<result_type>>(
            numOutstanding, handler, [i, &results](auto& result) {
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
    numReadRequestsOutstanding_ -= keys.size();

    for (auto const& cb : cbs)
    {
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
    boost::asio::yield_context& yield) const
{
    CassandraStatement statement{selectDiff_};
    statement.bindNextInt(ledgerSequence);
    auto start = std::chrono::system_clock::now();

    CassandraResult result = executeAsyncRead(statement, yield);

    auto end = std::chrono::system_clock::now();

    if (!result)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__
            << " - no rows . ledger = " << std::to_string(ledgerSequence);
        return {};
    }
    std::vector<ripple::uint256> keys;
    do
    {
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
        [](auto const& k, auto const& o) {
            return LedgerObject{k, o};
        });
    return results;
}

bool
CassandraBackend::doOnlineDelete(
    std::uint32_t const numLedgersToKeep,
    boost::asio::yield_context& yield) const
{
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
        std::tuple<ripple::uint256, std::uint32_t, Blob>,
        typename std::remove_reference<decltype(bind)>::type>>>
        cbs;
    std::uint32_t concurrentLimit = 10;
    std::atomic_int numOutstanding = 0;

    // iterate through latest ledger, updating TTL
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        auto [objects, curCursor] = retryOnTimeout([&]() {
            return fetchLedgerPage(cursor, minLedger, 256, false, yield);
        });

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
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [&numOutstanding]() { return numOutstanding == 0; });
    CassandraStatement statement{deleteLedgerRange_};
    statement.bindNextInt(minLedger);
    executeSyncWrite(statement);
    // update ledger_range
    return true;
}

bool
CassandraBackend::isTooBusy() const
{
    return numReadRequestsOutstanding_ >= maxReadRequestsOutstanding;
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
        BOOST_LOG_TRIVIAL(debug) << "user = " << username.c_str();
        cass_cluster_set_credentials(
            cluster, username.c_str(), getString("password").c_str());
    }
    int threads =
        getInt("threads").value_or(std::thread::hardware_concurrency());

    rc = cass_cluster_set_num_threads_io(cluster, threads);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra io threads to " << threads
           << ", result: " << rc << ", " << cass_error_desc(rc);
        throw std::runtime_error(ss.str());
    }
    if (getInt("max_write_requests_outstanding"))
        maxWriteRequestsOutstanding = *getInt("max_write_requests_outstanding");

    if (getInt("max_read_requests_outstanding"))
        maxReadRequestsOutstanding = *getInt("max_read_requests_outstanding");

    if (getInt("sync_interval"))
        syncInterval_ = *getInt("sync_interval");
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sync interval is " << syncInterval_
        << ". max write requests outstanding is " << maxWriteRequestsOutstanding
        << ". max read requests outstanding is " << maxReadRequestsOutstanding;

    cass_cluster_set_request_timeout(cluster, 10000);

    rc = cass_cluster_set_queue_size_io(
        cluster,
        maxWriteRequestsOutstanding +
            maxReadRequestsOutstanding);  // This number needs to scale w/ the
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
            << "No keyspace specified. Using keyspace clio";
        keyspace = "clio";
    }

    int rf = getInt("replication_factor") ? *getInt("replication_factor") : 3;

    std::string tablePrefix = getString("table_prefix");
    if (tablePrefix.empty())
    {
        BOOST_LOG_TRIVIAL(warning) << "Table prefix is empty";
    }

    cass_cluster_set_connect_timeout(cluster, 10000);

    int ttl = getInt("ttl") ? *getInt("ttl") * 2 : 0;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " setting ttl to " << std::to_string(ttl);

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

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "nf_tokens"
              << "  ("
              << "    token_id blob,"
              << "    sequence bigint,"
              << "    owner blob,"
              << "    is_burned boolean,"
              << "    PRIMARY KEY (token_id, sequence)"
              << "  )"
              << "  WITH CLUSTERING ORDER BY (sequence DESC)"
              << "    AND default_time_to_live = " << ttl;
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "nf_tokens"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix
              << "issuer_nf_tokens"
              << "  ("
              << "    issuer blob,"
              << "    token_id blob,"
              << "    PRIMARY KEY (issuer, token_id)"
              << "  )";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "issuer_nf_tokens"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix
              << "nf_token_transactions"
              << "  ("
              << "    token_id blob,"
              << "    seq_idx tuple<bigint, bigint>,"
              << "    hash blob,"
              << "    PRIMARY KEY (token_id, seq_idx)"
              << "  )"
              << "  WITH CLUSTERING ORDER BY (seq_idx DESC)"
              << "    AND default_time_to_live = " << ttl;
        if (!executeSimpleStatement(query.str()))
            continue;

        query.str("");
        query << "SELECT * FROM " << tablePrefix << "nf_token_transactions"
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
        query << "INSERT INTO " << tablePrefix << "nf_tokens"
              << " (token_id,sequence,owner,is_burned)"
              << " VALUES (?,?,?,?)";
        if (!insertNFT_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT sequence,owner,is_burned"
              << " FROM " << tablePrefix << "nf_tokens WHERE"
              << " token_id = ? AND"
              << " sequence <= ?"
              << " ORDER BY sequence DESC"
              << " LIMIT 1";
        if (!selectNFT_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "INSERT INTO " << tablePrefix << "issuer_nf_tokens"
              << " (issuer,token_id)"
              << " VALUES (?,?)";
        if (!insertIssuerNFT_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "INSERT INTO " << tablePrefix << "nf_token_transactions"
              << " (token_id,seq_idx,hash)"
              << " VALUES (?,?,?)";
        if (!insertNFTTx_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT hash,seq_idx"
              << " FROM " << tablePrefix << "nf_token_transactions WHERE"
              << " token_id = ? AND"
              << " seq_idx < ?"
              << " ORDER BY seq_idx DESC"
              << " LIMIT ?";
        if (!selectNFTTx_.prepareStatement(query, session_.get()))
            continue;

        query.str("");
        query << "SELECT hash,seq_idx"
              << " FROM " << tablePrefix << "nf_token_transactions WHERE"
              << " token_id = ? AND"
              << " seq_idx >= ?"
              << " ORDER BY seq_idx ASC"
              << " LIMIT ?";
        if (!selectNFTTxForward_.prepareStatement(query, session_.get()))
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
}  // namespace Backend
