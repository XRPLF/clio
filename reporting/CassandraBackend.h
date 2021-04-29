//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_APP_REPORTING_CASSANDRABACKEND_H_INCLUDED
#define RIPPLE_APP_REPORTING_CASSANDRABACKEND_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <atomic>
#include <cassandra.h>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>

namespace Backend {

void
flatMapWriteCallback(CassFuture* fut, void* cbData);
void
flatMapWriteKeyCallback(CassFuture* fut, void* cbData);
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData);
void
flatMapWriteBookCallback(CassFuture* fut, void* cbData);
void
flatMapWriteAccountTxCallback(CassFuture* fut, void* cbData);
void
flatMapReadCallback(CassFuture* fut, void* cbData);
void
flatMapReadObjectCallback(CassFuture* fut, void* cbData);
void
flatMapGetCreatedCallback(CassFuture* fut, void* cbData);
void
flatMapWriteLedgerHeaderCallback(CassFuture* fut, void* cbData);
void
flatMapWriteLedgerHashCallback(CassFuture* fut, void* cbData);

class CassandraPreparedStatement
{
private:
    CassPrepared const* prepared_ = nullptr;

public:
    CassPrepared const*
    get() const
    {
        return prepared_;
    }

    bool
    prepareStatement(std::stringstream const& query, CassSession* session)
    {
        return prepareStatement(query.str().c_str(), session);
    }

    bool
    prepareStatement(std::string const& query, CassSession* session)
    {
        return prepareStatement(query.c_str(), session);
    }

    bool
    prepareStatement(char const* query, CassSession* session)
    {
        if (!query)
            throw std::runtime_error("prepareStatement: null query");
        if (!session)
            throw std::runtime_error("prepareStatement: null sesssion");
        CassFuture* prepareFuture = cass_session_prepare(session, query);
        /* Wait for the statement to prepare and get the result */
        CassError rc = cass_future_error_code(prepareFuture);
        if (rc == CASS_OK)
        {
            prepared_ = cass_future_get_prepared(prepareFuture);
        }
        else
        {
            std::stringstream ss;
            ss << "nodestore: Error preparing statement : " << rc << ", "
               << cass_error_desc(rc) << ". query : " << query;
            BOOST_LOG_TRIVIAL(error) << ss.str();
        }
        cass_future_free(prepareFuture);
        return rc == CASS_OK;
    }

    ~CassandraPreparedStatement()
    {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        if (prepared_)
        {
            cass_prepared_free(prepared_);
            prepared_ = nullptr;
        }
    }
};

class CassandraStatement
{
    CassStatement* statement_ = nullptr;
    size_t curBindingIndex_ = 0;

public:
    CassandraStatement(CassandraPreparedStatement const& prepared)
    {
        statement_ = cass_prepared_bind(prepared.get());
        cass_statement_set_consistency(statement_, CASS_CONSISTENCY_QUORUM);
    }

    CassStatement*
    get() const
    {
        return statement_;
    }

    void
    bindBoolean(bool val)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindBoolean - statement_ is null");
        CassError rc = cass_statement_bind_bool(
            statement_, 1, static_cast<cass_bool_t>(val));
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding boolean to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindBytes(const char* data, uint32_t size)
    {
        bindBytes((unsigned char*)data, size);
    }

    void
    bindBytes(ripple::uint256 const& data)
    {
        bindBytes(data.data(), data.size());
    }
    void
    bindBytes(ripple::AccountID const& data)
    {
        bindBytes(data.data(), data.size());
    }

    void
    bindBytes(std::string const& data)
    {
        bindBytes(data.data(), data.size());
    }

    void
    bindBytes(void const* key, uint32_t size)
    {
        bindBytes(static_cast<const unsigned char*>(key), size);
    }

    void
    bindBytes(const unsigned char* data, uint32_t size)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindBytes - statement_ is null");
        CassError rc = cass_statement_bind_bytes(
            statement_,
            curBindingIndex_,
            static_cast<cass_byte_t const*>(data),
            size);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding bytes to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindUInt(uint32_t value)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindUInt - statement_ is null");
        BOOST_LOG_TRIVIAL(trace)
            << std::to_string(curBindingIndex_) << " " << std::to_string(value);
        CassError rc =
            cass_statement_bind_int32(statement_, curBindingIndex_, value);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding uint to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindInt(uint32_t value)
    {
        bindInt((int64_t)value);
    }

    void
    bindInt(int64_t value)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindInt - statement_ is null");
        CassError rc =
            cass_statement_bind_int64(statement_, curBindingIndex_, value);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindIntTuple(uint32_t first, uint32_t second)
    {
        CassTuple* tuple = cass_tuple_new(2);
        CassError rc = cass_tuple_set_int64(tuple, 0, first);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_tuple_set_int64(tuple, 1, second);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_tuple(statement_, curBindingIndex_, tuple);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding tuple to statement: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        cass_tuple_free(tuple);
        curBindingIndex_++;
    }

    ~CassandraStatement()
    {
        if (statement_)
            cass_statement_free(statement_);
    }
};

class CassandraResult
{
    CassResult const* result_ = nullptr;
    CassRow const* row_ = nullptr;
    CassIterator* iter_ = nullptr;
    size_t curGetIndex_ = 0;

public:
    CassandraResult() : result_(nullptr), row_(nullptr), iter_(nullptr)
    {
    }

    CassandraResult&
    operator=(CassandraResult&& other)
    {
        result_ = other.result_;
        row_ = other.row_;
        iter_ = other.iter_;
        curGetIndex_ = other.curGetIndex_;
        other.result_ = nullptr;
        other.row_ = nullptr;
        other.iter_ = nullptr;
        other.curGetIndex_ = 0;
        return *this;
    }

    CassandraResult(CassandraResult const& other) = delete;
    CassandraResult&
    operator=(CassandraResult const& other) = delete;

    CassandraResult(CassResult const* result) : result_(result)
    {
        if (!result_)
            throw std::runtime_error("CassandraResult - result is null");
        iter_ = cass_iterator_from_result(result_);
        if (cass_iterator_next(iter_))
        {
            row_ = cass_iterator_get_row(iter_);
        }
    }

    bool
    isOk()
    {
        return result_ != nullptr;
    }

    bool
    hasResult()
    {
        return row_ != nullptr;
    }

    bool
    operator!()
    {
        return !hasResult();
    }

    size_t
    numRows()
    {
        return cass_result_row_count(result_);
    }

    bool
    nextRow()
    {
        curGetIndex_ = 0;
        if (cass_iterator_next(iter_))
        {
            row_ = cass_iterator_get_row(iter_);
            return true;
        }
        row_ = nullptr;
        return false;
    }

    std::vector<unsigned char>
    getBytes()
    {
        if (!row_)
            throw std::runtime_error("CassandraResult::getBytes - no result");
        cass_byte_t const* buf;
        std::size_t bufSize;
        CassError rc = cass_value_get_bytes(
            cass_row_get_column(row_, curGetIndex_), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            std::stringstream msg;
            msg << "CassandraResult::getBytes - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        curGetIndex_++;
        return {buf, buf + bufSize};
    }
    /*
    uint32_t
    getNumBytes()
    {
        if (!row_)
            throw std::runtime_error("CassandraResult::getBytes - no result");
        cass_byte_t const* buf;
        std::size_t bufSize;
        CassError rc = cass_value_get_bytes(
            cass_row_get_column(row_, curGetIndex_), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            std::stringstream msg;
            msg << "CassandraResult::getBytes - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        return bufSize;
    }*/

    ripple::uint256
    getUInt256()
    {
        if (!row_)
            throw std::runtime_error("CassandraResult::uint256 - no result");
        cass_byte_t const* buf;
        std::size_t bufSize;
        CassError rc = cass_value_get_bytes(
            cass_row_get_column(row_, curGetIndex_), &buf, &bufSize);
        if (rc != CASS_OK)
        {
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
    getInt64()
    {
        if (!row_)
            throw std::runtime_error("CassandraResult::getInt64 - no result");
        cass_int64_t val;
        CassError rc =
            cass_value_get_int64(cass_row_get_column(row_, curGetIndex_), &val);
        if (rc != CASS_OK)
        {
            std::stringstream msg;
            msg << "CassandraResult::getInt64 - error getting value: " << rc
                << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << msg.str();
            throw std::runtime_error(msg.str());
        }
        ++curGetIndex_;
        return val;
    }

    uint32_t
    getUInt32()
    {
        return (uint32_t)getInt64();
    }

    std::pair<int64_t, int64_t>
    getInt64Tuple()
    {
        if (!row_)
            throw std::runtime_error(
                "CassandraResult::getInt64Tuple - no result");
        CassValue const* tuple = cass_row_get_column(row_, curGetIndex_);
        CassIterator* tupleIter = cass_iterator_from_tuple(tuple);
        if (!cass_iterator_next(tupleIter))
            throw std::runtime_error(
                "CassandraResult::getInt64Tuple - failed to iterate tuple");
        CassValue const* value = cass_iterator_get_value(tupleIter);
        int64_t first;
        cass_value_get_int64(value, &first);
        if (!cass_iterator_next(tupleIter))
            throw std::runtime_error(
                "CassandraResult::getInt64Tuple - failed to iterate tuple");
        value = cass_iterator_get_value(tupleIter);
        int64_t second;
        cass_value_get_int64(value, &second);
        ++curGetIndex_;
        return {first, second};
    }

    ~CassandraResult()
    {
        if (result_ != nullptr)
            cass_result_free(result_);
        if (iter_ != nullptr)
            cass_iterator_free(iter_);
    }
};
inline bool
isTimeout(CassError rc)
{
    if (rc == CASS_ERROR_LIB_NO_HOSTS_AVAILABLE or
        rc == CASS_ERROR_LIB_REQUEST_TIMED_OUT or
        rc == CASS_ERROR_SERVER_UNAVAILABLE or
        rc == CASS_ERROR_SERVER_OVERLOADED or
        rc == CASS_ERROR_SERVER_READ_TIMEOUT)
        return true;
    return false;
}
template <class T, class F>
class CassandraAsyncResult
{
    T& requestParams_;
    CassandraResult result_;
    bool timedOut_ = false;
    bool retryOnTimeout_ = false;

public:
    CassandraAsyncResult(
        T& requestParams,
        CassFuture* fut,
        F retry,
        bool retryOnTimeout = false)
        : requestParams_(requestParams), retryOnTimeout_(retryOnTimeout)
    {
        CassError rc = cass_future_error_code(fut);
        if (rc != CASS_OK)
        {
            // TODO - should we ever be retrying requests? These are reads,
            // and they usually only fail when the db is under heavy load. Seems
            // best to just return an error to the client and have the client
            // try again
            if (isTimeout(rc))
                timedOut_ = true;
            if (!timedOut_ || retryOnTimeout_)
                retry(requestParams_);
        }
        else
        {
            result_ = std::move(CassandraResult(cass_future_get_result(fut)));
        }
    }

    ~CassandraAsyncResult()
    {
        if (result_.isOk() or timedOut_)
        {
            BOOST_LOG_TRIVIAL(trace) << "finished a request";
            size_t batchSize = requestParams_.batchSize;
            if (++(requestParams_.numFinished) == batchSize)
                requestParams_.cv.notify_all();
        }
    }

    CassandraResult&
    getResult()
    {
        return result_;
    }

    bool
    timedOut()
    {
        return timedOut_;
    }
};

class CassandraBackend : public BackendInterface
{
private:
    // convenience function for one-off queries. For normal reads and writes,
    // use the prepared statements insert_ and select_
    CassStatement*
    makeStatement(char const* query, std::size_t params)
    {
        CassStatement* ret = cass_statement_new(query, params);
        CassError rc =
            cass_statement_set_consistency(ret, CASS_CONSISTENCY_QUORUM);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting query consistency: " << query
               << ", result: " << rc << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }
        return ret;
    }

    std::atomic<bool> open_{false};

    // mutex used for open() and close()
    std::mutex mutex_;

    std::unique_ptr<CassSession, void (*)(CassSession*)> session_{
        nullptr,
        [](CassSession* session) {
            // Try to disconnect gracefully.
            CassFuture* fut = cass_session_close(session);
            cass_future_wait(fut);
            cass_future_free(fut);
            cass_session_free(session);
        }};

    // Database statements cached server side. Using these is more efficient
    // than making a new statement
    CassandraPreparedStatement insertObject_;
    CassandraPreparedStatement insertTransaction_;
    CassandraPreparedStatement selectTransaction_;
    CassandraPreparedStatement selectAllTransactionsInLedger_;
    CassandraPreparedStatement selectAllTransactionHashesInLedger_;
    CassandraPreparedStatement selectObject_;
    CassandraPreparedStatement selectLedgerPageKeys_;
    CassandraPreparedStatement selectLedgerPage_;
    CassandraPreparedStatement upperBound2_;
    CassandraPreparedStatement getToken_;
    CassandraPreparedStatement insertKey_;
    CassandraPreparedStatement selectKeys_;
    CassandraPreparedStatement getBook_;
    CassandraPreparedStatement selectBook_;
    CassandraPreparedStatement insertBook_;
    CassandraPreparedStatement insertBook2_;
    CassandraPreparedStatement deleteBook_;
    CassandraPreparedStatement insertAccountTx_;
    CassandraPreparedStatement selectAccountTx_;
    CassandraPreparedStatement insertLedgerHeader_;
    CassandraPreparedStatement insertLedgerHash_;
    CassandraPreparedStatement updateLedgerRange_;
    CassandraPreparedStatement updateLedgerHeader_;
    CassandraPreparedStatement selectLedgerBySeq_;
    CassandraPreparedStatement selectLatestLedger_;
    CassandraPreparedStatement selectLedgerRange_;
    CassandraPreparedStatement selectLedgerDiff_;

    // io_context used for exponential backoff for write retries
    mutable boost::asio::io_context ioContext_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    // maximum number of concurrent in flight requests. New requests will wait
    // for earlier requests to finish if this limit is exceeded
    uint32_t maxRequestsOutstanding = 10000;
    mutable std::atomic_uint32_t numRequestsOutstanding_ = 0;

    // mutex and condition_variable to limit the number of concurrent in flight
    // requests
    mutable std::mutex throttleMutex_;
    mutable std::condition_variable throttleCv_;

    // writes are asynchronous. This mutex and condition_variable is used to
    // wait for all writes to finish
    mutable std::mutex syncMutex_;
    mutable std::condition_variable syncCv_;

    boost::json::object config_;

    mutable uint32_t ledgerSequence_ = 0;
    mutable bool isFirstLedger_ = false;

public:
    CassandraBackend(boost::json::object const& config)
        : BackendInterface(config), config_(config)
    {
    }

    ~CassandraBackend() override
    {
        BOOST_LOG_TRIVIAL(info) << __func__;
        if (open_)
            close();
    }

    std::string
    getName()
    {
        return "cassandra";
    }

    bool
    isOpen()
    {
        return open_;
    }

    // Setup all of the necessary components for talking to the database.
    // Create the table if it doesn't exist already
    // @param createIfMissing ignored
    void
    open(bool readOnly) override;

    // Close the connection to the database
    void
    close() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            work_.reset();
            ioThread_.join();
        }
        open_ = false;
    }
    CassandraPreparedStatement const&
    getInsertKeyPreparedStatement() const
    {
        return insertKey_;
    }
    CassandraPreparedStatement const&
    getInsertBookPreparedStatement() const
    {
        return insertBook2_;
    }

    CassandraPreparedStatement const&
    getSelectLedgerDiffPreparedStatement() const
    {
        return selectLedgerDiff_;
    }

    std::pair<
        std::vector<TransactionAndMetadata>,
        std::optional<AccountTransactionsCursor>>
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        std::optional<AccountTransactionsCursor> const& cursor) const override
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doAccountTx";
        CassandraStatement statement{selectAccountTx_};
        statement.bindBytes(account);
        if (cursor)
            statement.bindIntTuple(
                cursor->ledgerSequence, cursor->transactionIndex);
        else
            statement.bindIntTuple(INT32_MAX, INT32_MAX);
        statement.bindUInt(limit);
        CassandraResult result = executeSyncRead(statement);
        if (!result.hasResult())
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - no rows returned";
            return {{}, {}};
        }

        std::vector<ripple::uint256> hashes;
        size_t numRows = result.numRows();
        bool returnCursor = numRows == limit;
        std::optional<AccountTransactionsCursor> retCursor;

        BOOST_LOG_TRIVIAL(info) << "num_rows = " << std::to_string(numRows);
        do
        {
            hashes.push_back(result.getUInt256());
            --numRows;
            if (numRows == 0 && returnCursor)
            {
                auto [lgrSeq, txnIdx] = result.getInt64Tuple();
                retCursor = {(uint32_t)lgrSeq, (uint32_t)txnIdx};
            }
        } while (result.nextRow());

        BOOST_LOG_TRIVIAL(debug)
            << "doAccountTx - populated hashes. num hashes = " << hashes.size();
        if (hashes.size())
        {
            return {fetchTransactions(hashes), retCursor};
        }
        return {{}, {}};
    }

    struct WriteLedgerHeaderCallbackData
    {
        CassandraBackend const* backend;
        uint32_t sequence;
        std::string header;
        uint32_t currentRetries = 0;

        std::atomic<int> refs = 1;
        WriteLedgerHeaderCallbackData(
            CassandraBackend const* f,
            uint32_t sequence,
            std::string&& header)
            : backend(f), sequence(sequence), header(std::move(header))
        {
        }
    };
    struct WriteLedgerHashCallbackData
    {
        CassandraBackend const* backend;
        ripple::uint256 hash;
        uint32_t sequence;
        uint32_t currentRetries = 0;

        std::atomic<int> refs = 1;
        WriteLedgerHashCallbackData(
            CassandraBackend const* f,
            ripple::uint256 hash,
            uint32_t sequence)
            : backend(f), hash(hash), sequence(sequence)
        {
        }
    };

    bool
    doFinishWrites() const override
    {
        // wait for all other writes to finish
        sync();
        // write range
        if (isFirstLedger_)
        {
            CassandraStatement statement{updateLedgerRange_};
            statement.bindInt(ledgerSequence_);
            statement.bindBoolean(false);
            statement.bindInt(ledgerSequence_);
            executeSyncWrite(statement);
        }
        CassandraStatement statement{updateLedgerRange_};
        statement.bindInt(ledgerSequence_);
        statement.bindBoolean(true);
        statement.bindInt(ledgerSequence_ - 1);
        return executeSyncUpdate(statement);
    }
    void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& header,
        bool isFirst = false) const override
    {
        WriteLedgerHeaderCallbackData* headerCb =
            new WriteLedgerHeaderCallbackData(
                this, ledgerInfo.seq, std::move(header));
        WriteLedgerHashCallbackData* hashCb = new WriteLedgerHashCallbackData(
            this, ledgerInfo.hash, ledgerInfo.seq);
        writeLedgerHeader(*headerCb, false);
        writeLedgerHash(*hashCb, false);
        ledgerSequence_ = ledgerInfo.seq;
        isFirstLedger_ = isFirst;
    }
    void
    writeLedgerHash(WriteLedgerHashCallbackData& cb, bool isRetry) const
    {
        CassandraStatement statement{insertLedgerHash_};
        statement.bindBytes(cb.hash);
        statement.bindInt(cb.sequence);
        executeAsyncWrite(
            statement, flatMapWriteLedgerHashCallback, cb, isRetry);
    }

    void
    writeLedgerHeader(WriteLedgerHeaderCallbackData& cb, bool isRetry) const
    {
        CassandraStatement statement{insertLedgerHeader_};
        statement.bindInt(cb.sequence);
        statement.bindBytes(cb.header);
        executeAsyncWrite(
            statement, flatMapWriteLedgerHeaderCallback, cb, isRetry);
    }

    std::optional<uint32_t>
    fetchLatestLedgerSequence() const override
    {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectLatestLedger_};
        CassandraResult result = executeSyncRead(statement);
        if (!result.hasResult())
        {
            BOOST_LOG_TRIVIAL(error)
                << "CassandraBackend::fetchLatestLedgerSequence - no rows";
            return {};
        }
        return result.getUInt32();
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const override
    {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectLedgerBySeq_};
        statement.bindInt(sequence);
        CassandraResult result = executeSyncRead(statement);

        if (!result)
        {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        std::vector<unsigned char> header = result.getBytes();
        return deserializeHeader(ripple::makeSlice(header));
    }
    std::optional<LedgerRange>
    fetchLedgerRange() const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const override;

    // Synchronously fetch the object with key key and store the result in
    // pno
    // @param key the key of the object
    // @param pno object in which to store the result
    // @return result status of query
    std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence)
        const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{selectObject_};
        statement.bindBytes(key);
        statement.bindInt(sequence);
        CassandraResult result = executeSyncRead(statement);
        if (!result)
        {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        return result.getBytes();
    }

    std::optional<int64_t>
    getToken(void const* key) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        CassandraStatement statement{getToken_};
        statement.bindBytes(key, 32);
        CassandraResult result = executeSyncRead(statement);
        if (!result)
        {
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
    fetchTransaction(ripple::uint256 const& hash) const override
    {
        BOOST_LOG_TRIVIAL(trace) << __func__;
        CassandraStatement statement{selectTransaction_};
        statement.bindBytes(hash);
        CassandraResult result = executeSyncRead(statement);
        if (!result)
        {
            BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
            return {};
        }
        return {{result.getBytes(), result.getBytes(), result.getUInt32()}};
    }
    LedgerPage
    fetchLedgerPage2(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const;
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const override;
    std::vector<LedgerObject>
    fetchLedgerDiff(uint32_t ledgerSequence) const;
    std::map<uint32_t, std::vector<LedgerObject>>
    fetchLedgerDiffs(std::vector<uint32_t> const& sequences) const;

    bool
    runIndexer(uint32_t ledgerSequence) const;
    bool
    isIndexed(uint32_t ledgerSequence) const;

    std::optional<uint32_t>
    getNextToIndex() const;

    bool
    writeKeys(
        std::unordered_set<ripple::uint256> const& keys,
        uint32_t ledgerSequence) const;
    bool
    writeBooks(
        std::unordered_map<
            ripple::uint256,
            std::unordered_set<ripple::uint256>> const& books,
        uint32_t ledgerSequence) const override;
    std::pair<std::vector<LedgerObject>, std::optional<ripple::uint256>>
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t sequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor) const override;

    std::pair<std::vector<LedgerObject>, std::optional<ripple::uint256>>
    fetchBookOffers2(
        ripple::uint256 const& book,
        uint32_t sequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor) const
    {
        CassandraStatement statement{getBook_};
        statement.bindBytes(book);
        statement.bindInt(sequence);
        statement.bindInt(sequence);
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
                results.push_back({keys[i], objs[i]});
            }
            return {results, results[results.size() - 1].key};
        }

        return {{}, {}};
    }
    bool
    canFetchBatch()
    {
        return true;
    }

    struct ReadCallbackData
    {
        CassandraBackend const& backend;
        ripple::uint256 const& hash;
        TransactionAndMetadata& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadCallbackData(
            CassandraBackend const& backend,
            ripple::uint256 const& hash,
            TransactionAndMetadata& result,
            std::condition_variable& cv,
            std::atomic_uint32_t& numFinished,
            size_t batchSize)
            : backend(backend)
            , hash(hash)
            , result(result)
            , cv(cv)
            , numFinished(numFinished)
            , batchSize(batchSize)
        {
        }

        ReadCallbackData(ReadCallbackData const& other) = default;
    };

    std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes) const override
    {
        std::size_t const numHashes = hashes.size();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetching " << numHashes << " transactions from Cassandra";
        std::atomic_uint32_t numFinished = 0;
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<TransactionAndMetadata> results{numHashes};
        std::vector<std::shared_ptr<ReadCallbackData>> cbs;
        cbs.reserve(numHashes);
        for (std::size_t i = 0; i < hashes.size(); ++i)
        {
            cbs.push_back(std::make_shared<ReadCallbackData>(
                *this, hashes[i], results[i], cv, numFinished, numHashes));
            read(*cbs[i]);
        }
        assert(results.size() == cbs.size());

        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [&numFinished, &numHashes]() {
            return numFinished == numHashes;
        });
        for (auto const& res : results)
        {
            if (res.transaction.size() == 1 && res.transaction[0] == 0)
                throw DatabaseTimeout();
        }

        BOOST_LOG_TRIVIAL(debug)
            << "Fetched " << numHashes << " transactions from Cassandra";
        return results;
    }

    void
    read(ReadCallbackData& data) const
    {
        CassandraStatement statement{selectTransaction_};
        statement.bindBytes(data.hash);
        executeAsyncRead(statement, flatMapReadCallback, data);
    }

    struct ReadObjectCallbackData
    {
        CassandraBackend const& backend;
        ripple::uint256 const& key;
        uint32_t sequence;
        Blob& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadObjectCallbackData(
            CassandraBackend const& backend,
            ripple::uint256 const& key,
            uint32_t sequence,
            Blob& result,
            std::condition_variable& cv,
            std::atomic_uint32_t& numFinished,
            size_t batchSize)
            : backend(backend)
            , key(key)
            , sequence(sequence)
            , result(result)
            , cv(cv)
            , numFinished(numFinished)
            , batchSize(batchSize)
        {
        }

        ReadObjectCallbackData(ReadObjectCallbackData const& other) = default;
    };

    void
    readObject(ReadObjectCallbackData& data) const
    {
        CassandraStatement statement{selectObject_};
        statement.bindBytes(data.key);
        statement.bindInt(data.sequence);

        executeAsyncRead(statement, flatMapReadObjectCallback, data);
    }
    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const override;

    struct WriteCallbackData
    {
        CassandraBackend const* backend;
        std::string key;
        uint32_t sequence;
        uint32_t createdSequence = 0;
        std::string blob;
        bool isCreated;
        bool isDeleted;
        std::optional<ripple::uint256> book;

        uint32_t currentRetries = 0;
        std::atomic<int> refs = 1;

        WriteCallbackData(
            CassandraBackend const* f,
            std::string&& key,
            uint32_t sequence,
            std::string&& blob,
            bool isCreated,
            bool isDeleted,
            std::optional<ripple::uint256>&& inBook)
            : backend(f)
            , key(std::move(key))
            , sequence(sequence)
            , blob(std::move(blob))
            , isCreated(isCreated)
            , isDeleted(isDeleted)
            , book(std::move(inBook))
        {
        }
    };
    struct WriteAccountTxCallbackData
    {
        CassandraBackend const* backend;
        AccountTransactionsData data;

        uint32_t currentRetries = 0;
        std::atomic<int> refs;

        WriteAccountTxCallbackData(
            CassandraBackend const* f,
            AccountTransactionsData&& in)
            : backend(f), data(std::move(in)), refs(data.accounts.size())
        {
        }
    };

    void
    write(WriteCallbackData& data, bool isRetry) const
    {
        {
            CassandraStatement statement{insertObject_};
            statement.bindBytes(data.key);
            statement.bindInt(data.sequence);
            statement.bindBytes(data.blob);

            executeAsyncWrite(statement, flatMapWriteCallback, data, isRetry);
        }
    }

    /*
    void
    writeDeletedKey(WriteCallbackData& data, bool isRetry) const
    {
        CassandraStatement statement{insertKey_};
        statement.bindBytes(data.key);
        statement.bindInt(data.createdSequence);
        statement.bindInt(data.sequence);
        executeAsyncWrite(statement, flatMapWriteKeyCallback, data, isRetry);
    }

    void
    writeKey(WriteCallbackData& data, bool isRetry) const
    {
        if (data.isCreated)
        {
            CassandraStatement statement{insertKey_};
            statement.bindBytes(data.key);
            statement.bindInt(data.sequence);
            statement.bindInt(INT64_MAX);

            executeAsyncWrite(
                statement, flatMapWriteKeyCallback, data, isRetry);
        }
        else if (data.isDeleted)
        {
            CassandraStatement statement{getCreated_};

            executeAsyncWrite(
                statement, flatMapGetCreatedCallback, data, isRetry);
        }
    }
    */

    void
    writeBook(WriteCallbackData& data, bool isRetry) const
    {
        assert(data.isCreated or data.isDeleted);
        assert(data.book);
        CassandraStatement statement{
            (data.isCreated ? insertBook_ : deleteBook_)};
        statement.bindBytes(*data.book);
        statement.bindBytes(data.key);
        statement.bindInt(data.sequence);
        if (data.isCreated)
            statement.bindInt(INT64_MAX);
        executeAsyncWrite(statement, flatMapWriteBookCallback, data, isRetry);
    }
    void
    doWriteLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
        bool hasBook = book.has_value();
        WriteCallbackData* data = new WriteCallbackData(
            this,
            std::move(key),
            seq,
            std::move(blob),
            isCreated,
            isDeleted,
            std::move(book));

        write(*data, false);
    }

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) const override
    {
        for (auto& record : data)
        {
            WriteAccountTxCallbackData* cbData =
                new WriteAccountTxCallbackData(this, std::move(record));
            writeAccountTx(*cbData, false);
        }
    }

    void
    writeAccountTx(WriteAccountTxCallbackData& data, bool isRetry) const
    {
        for (auto const& account : data.data.accounts)
        {
            CassandraStatement statement(insertAccountTx_);
            statement.bindBytes(account);
            statement.bindIntTuple(
                data.data.ledgerSequence, data.data.transactionIndex);
            statement.bindBytes(data.data.txHash);

            executeAsyncWrite(
                statement, flatMapWriteAccountTxCallback, data, isRetry);
        }
    }

    struct WriteTransactionCallbackData
    {
        CassandraBackend const* backend;
        // The shared pointer to the node object must exist until it's
        // confirmed persisted. Otherwise, it can become deleted
        // prematurely if other copies are removed from caches.
        std::string hash;
        uint32_t sequence;
        std::string transaction;
        std::string metadata;

        uint32_t currentRetries = 0;

        std::atomic<int> refs = 1;
        WriteTransactionCallbackData(
            CassandraBackend const* f,
            std::string&& hash,
            uint32_t sequence,
            std::string&& transaction,
            std::string&& metadata)
            : backend(f)
            , hash(std::move(hash))
            , sequence(sequence)
            , transaction(std::move(transaction))
            , metadata(std::move(metadata))
        {
        }
    };

    void
    writeTransaction(WriteTransactionCallbackData& data, bool isRetry) const
    {
        CassandraStatement statement{insertTransaction_};
        statement.bindBytes(data.hash);
        statement.bindInt(data.sequence);
        statement.bindBytes(data.transaction);
        statement.bindBytes(data.metadata);

        executeAsyncWrite(
            statement, flatMapWriteTransactionCallback, data, isRetry);
    }
    void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        std::string&& transaction,
        std::string&& metadata) const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Writing txn to cassandra";
        WriteTransactionCallbackData* data = new WriteTransactionCallbackData(
            this,
            std::move(hash),
            seq,
            std::move(transaction),
            std::move(metadata));

        writeTransaction(*data, false);
    }

    void
    startWrites() const override
    {
    }

    void
    sync() const
    {
        std::unique_lock<std::mutex> lck(syncMutex_);

        syncCv_.wait(lck, [this]() { return finishedAllRequests(); });
    }
    bool
    doOnlineDelete(uint32_t minLedgerToKeep) const override;

    boost::asio::io_context&
    getIOContext() const
    {
        return ioContext_;
    }

    friend void
    flatMapWriteCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteKeyCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteTransactionCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteBookCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteAccountTxCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteLedgerHeaderCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteLedgerHashCallback(CassFuture* fut, void* cbData);

    friend void
    flatMapReadCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapReadObjectCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapGetCreatedCallback(CassFuture* fut, void* cbData);

    inline void
    incremementOutstandingRequestCount() const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!canAddRequest())
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() { return canAddRequest(); });
            }
        }
        ++numRequestsOutstanding_;
    }

    inline void
    decrementOutstandingRequestCount() const
    {
        // sanity check
        if (numRequestsOutstanding_ == 0)
        {
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
        if (cur == 0)
        {
            // mutex lock required to prevent race condition around spurious
            // wakeup
            std::lock_guard lck(syncMutex_);
            syncCv_.notify_one();
        }
    }

    inline bool
    canAddRequest() const
    {
        return numRequestsOutstanding_ < maxRequestsOutstanding;
    }
    inline bool
    finishedAllRequests() const
    {
        return numRequestsOutstanding_ == 0;
    }

    void
    finishAsyncWrite() const
    {
        decrementOutstandingRequestCount();
    }

    template <class T, class S>
    void
    executeAsyncHelper(
        CassandraStatement const& statement,
        T callback,
        S& callbackData) const
    {
        CassFuture* fut = cass_session_execute(session_.get(), statement.get());

        cass_future_set_callback(
            fut, callback, static_cast<void*>(&callbackData));
        cass_future_free(fut);
    }
    template <class T, class S>
    void
    executeAsyncWrite(
        CassandraStatement const& statement,
        T callback,
        S& callbackData,
        bool isRetry) const
    {
        if (!isRetry)
            incremementOutstandingRequestCount();
        executeAsyncHelper(statement, callback, callbackData);
    }
    template <class T, class S>
    void
    executeAsyncRead(
        CassandraStatement const& statement,
        T callback,
        S& callbackData) const
    {
        executeAsyncHelper(statement, callback, callbackData);
    }
    void
    executeSyncWrite(CassandraStatement const& statement) const
    {
        CassFuture* fut;
        CassError rc;
        do
        {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
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
    executeSyncUpdate(CassandraStatement const& statement) const
    {
        CassFuture* fut;
        CassError rc;
        do
        {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra sync write error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } while (rc != CASS_OK);
        CassResult const* res = cass_future_get_result(fut);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "executeSyncUpdate - no rows";
            cass_result_free(res);
            return false;
        }
        cass_bool_t success;
        rc = cass_value_get_bool(cass_row_get_column(row, 0), &success);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "executeSyncUpdate - error getting result " << rc << ", "
                << cass_error_desc(rc);
            return false;
        }
        cass_result_free(res);
        return success == cass_true;
    }

    CassandraResult
    executeSyncRead(CassandraStatement const& statement) const
    {
        CassFuture* fut;
        CassError rc;
        do
        {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra executeSyncRead error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            if (isTimeout(rc))
            {
                cass_future_free(fut);
                throw DatabaseTimeout();
            }

            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                throw std::runtime_error("invalid query");
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_future_free(fut);
        return {res};
    }
};

}  // namespace Backend
#endif
