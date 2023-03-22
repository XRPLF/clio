//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <ripple/basics/base_uint.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <log/Logger.h>

#include <cassandra.h>

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>

#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <config/Config.h>

namespace Backend {

class CassandraPreparedStatement
{
private:
    clio::Logger log_{"Backend"};
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
            log_.error() << ss.str();
        }
        cass_future_free(prepareFuture);
        return rc == CASS_OK;
    }

    ~CassandraPreparedStatement()
    {
        // log_.trace() << "called";
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
    clio::Logger log_{"Backend"};

public:
    CassandraStatement(CassandraPreparedStatement const& prepared)
    {
        statement_ = cass_prepared_bind(prepared.get());
        cass_statement_set_consistency(statement_, CASS_CONSISTENCY_QUORUM);
    }

    CassandraStatement(CassandraStatement&& other)
    {
        statement_ = other.statement_;
        other.statement_ = nullptr;
        curBindingIndex_ = other.curBindingIndex_;
        other.curBindingIndex_ = 0;
    }

    CassandraStatement(CassandraStatement const& other) = delete;

    CassStatement*
    get() const
    {
        return statement_;
    }

    void
    bindNextBoolean(bool val)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindNextBoolean - statement_ is null");
        CassError rc = cass_statement_bind_bool(
            statement_, curBindingIndex_, static_cast<cass_bool_t>(val));
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding boolean to statement: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindNextBytes(const char* data, std::uint32_t const size)
    {
        bindNextBytes((unsigned const char*)(data), size);
    }

    void
    bindNextBytes(ripple::uint256 const& data)
    {
        bindNextBytes(data.data(), data.size());
    }
    void
    bindNextBytes(std::vector<unsigned char> const& data)
    {
        bindNextBytes(data.data(), data.size());
    }
    void
    bindNextBytes(ripple::AccountID const& data)
    {
        bindNextBytes(data.data(), data.size());
    }

    void
    bindNextBytes(std::string const& data)
    {
        bindNextBytes(data.data(), data.size());
    }

    void
    bindNextBytes(void const* key, std::uint32_t const size)
    {
        bindNextBytes(static_cast<const unsigned char*>(key), size);
    }

    void
    bindNextBytes(const unsigned char* data, std::uint32_t const size)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindNextBytes - statement_ is null");
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
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindNextUInt(std::uint32_t const value)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindNextUInt - statement_ is null");
        log_.trace() << std::to_string(curBindingIndex_) << " "
                     << std::to_string(value);
        CassError rc =
            cass_statement_bind_int32(statement_, curBindingIndex_, value);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding uint to statement: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindNextInt(std::uint32_t const value)
    {
        bindNextInt(static_cast<std::int64_t>(value));
    }

    void
    bindNextInt(int64_t value)
    {
        if (!statement_)
            throw std::runtime_error(
                "CassandraStatement::bindNextInt - statement_ is null");
        CassError rc =
            cass_statement_bind_int64(statement_, curBindingIndex_, value);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to statement: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        curBindingIndex_++;
    }

    void
    bindNextIntTuple(std::uint32_t const first, std::uint32_t const second)
    {
        CassTuple* tuple = cass_tuple_new(2);
        CassError rc = cass_tuple_set_int64(tuple, 0, first);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_tuple_set_int64(tuple, 1, second);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding int to tuple: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_tuple(statement_, curBindingIndex_, tuple);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "Error binding tuple to statement: " << rc << ", "
               << cass_error_desc(rc);
            log_.error() << ss.str();
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
    clio::Logger log_{"Backend"};
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
            log_.error() << msg.str();
            throw std::runtime_error(msg.str());
        }
        curGetIndex_++;
        return {buf, buf + bufSize};
    }

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
            log_.error() << msg.str();
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
            log_.error() << msg.str();
            throw std::runtime_error(msg.str());
        }
        ++curGetIndex_;
        return val;
    }

    std::uint32_t
    getUInt32()
    {
        return static_cast<std::uint32_t>(getInt64());
    }

    std::pair<std::int64_t, std::int64_t>
    getInt64Tuple()
    {
        if (!row_)
            throw std::runtime_error(
                "CassandraResult::getInt64Tuple - no result");

        CassValue const* tuple = cass_row_get_column(row_, curGetIndex_);
        CassIterator* tupleIter = cass_iterator_from_tuple(tuple);

        if (!cass_iterator_next(tupleIter))
        {
            cass_iterator_free(tupleIter);
            throw std::runtime_error(
                "CassandraResult::getInt64Tuple - failed to iterate tuple");
        }

        CassValue const* value = cass_iterator_get_value(tupleIter);
        std::int64_t first;
        cass_value_get_int64(value, &first);
        if (!cass_iterator_next(tupleIter))
        {
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
    getBytesTuple()
    {
        cass_byte_t const* buf;
        std::size_t bufSize;

        if (!row_)
            throw std::runtime_error(
                "CassandraResult::getBytesTuple - no result");
        CassValue const* tuple = cass_row_get_column(row_, curGetIndex_);
        CassIterator* tupleIter = cass_iterator_from_tuple(tuple);
        if (!cass_iterator_next(tupleIter))
            throw std::runtime_error(
                "CassandraResult::getBytesTuple - failed to iterate tuple");
        CassValue const* value = cass_iterator_get_value(tupleIter);
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

    // TODO: should be replaced with a templated implementation as is very
    // similar to other getters
    bool
    getBool()
    {
        if (!row_)
        {
            std::string msg{"No result"};
            log_.error() << msg;
            throw std::runtime_error(msg);
        }
        cass_bool_t val;
        CassError rc =
            cass_value_get_bool(cass_row_get_column(row_, curGetIndex_), &val);
        if (rc != CASS_OK)
        {
            std::stringstream msg;
            msg << "Error getting value: " << rc << ", " << cass_error_desc(rc);
            log_.error() << msg.str();
            throw std::runtime_error(msg.str());
        }
        ++curGetIndex_;
        return val;
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

template <typename CompletionToken>
CassError
cass_future_error_code(CassFuture* fut, CompletionToken&& token)
{
    using function_type = void(boost::system::error_code, CassError);
    using result_type =
        boost::asio::async_result<CompletionToken, function_type>;
    using handler_type = typename result_type::completion_handler_type;

    handler_type handler(std::forward<decltype(token)>(token));
    result_type result(handler);

    struct HandlerWrapper
    {
        handler_type handler;

        HandlerWrapper(handler_type&& handler_) : handler(std::move(handler_))
        {
        }
    };

    auto resume = [](CassFuture* fut, void* data) -> void {
        HandlerWrapper* hw = (HandlerWrapper*)data;

        boost::asio::post(
            boost::asio::get_associated_executor(hw->handler),
            [fut, hw, handler = std::move(hw->handler)]() mutable {
                delete hw;

                handler(
                    boost::system::error_code{}, cass_future_error_code(fut));
            });
    };

    HandlerWrapper* wrapper = new HandlerWrapper(std::move(handler));

    cass_future_set_callback(fut, resume, wrapper);

    // Suspend the coroutine until completion handler is called.
    // The handler will populate rc, the error code describing
    // the state of the cassandra future.
    auto rc = result.get();

    return rc;
}

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

    clio::Logger log_{"Backend"};
    std::atomic<bool> open_{false};

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
    CassandraPreparedStatement insertLedgerTransaction_;
    CassandraPreparedStatement selectTransaction_;
    CassandraPreparedStatement selectAllTransactionHashesInLedger_;
    CassandraPreparedStatement selectObject_;
    CassandraPreparedStatement selectLedgerPageKeys_;
    CassandraPreparedStatement selectLedgerPage_;
    CassandraPreparedStatement upperBound2_;
    CassandraPreparedStatement getToken_;
    CassandraPreparedStatement insertSuccessor_;
    CassandraPreparedStatement selectSuccessor_;
    CassandraPreparedStatement insertDiff_;
    CassandraPreparedStatement selectDiff_;
    CassandraPreparedStatement insertAccountTx_;
    CassandraPreparedStatement selectAccountTx_;
    CassandraPreparedStatement selectAccountTxForward_;
    CassandraPreparedStatement insertNFT_;
    CassandraPreparedStatement selectNFT_;
    CassandraPreparedStatement insertIssuerNFT_;
    CassandraPreparedStatement insertNFTURI_;
    CassandraPreparedStatement selectNFTURI_;
    CassandraPreparedStatement insertNFTTx_;
    CassandraPreparedStatement selectNFTTx_;
    CassandraPreparedStatement selectNFTTxForward_;
    CassandraPreparedStatement insertLedgerHeader_;
    CassandraPreparedStatement insertLedgerHash_;
    CassandraPreparedStatement updateLedgerRange_;
    CassandraPreparedStatement deleteLedgerRange_;
    CassandraPreparedStatement updateLedgerHeader_;
    CassandraPreparedStatement selectLedgerBySeq_;
    CassandraPreparedStatement selectLedgerByHash_;
    CassandraPreparedStatement selectLatestLedger_;
    CassandraPreparedStatement selectLedgerRange_;

    uint32_t syncInterval_ = 1;
    uint32_t lastSync_ = 0;

    // maximum number of concurrent in flight write requests. New requests will
    // wait for earlier requests to finish if this limit is exceeded
    std::uint32_t maxWriteRequestsOutstanding = 10000;
    mutable std::atomic_uint32_t numWriteRequestsOutstanding_ = 0;

    // maximum number of concurrent in flight read requests. isTooBusy() will
    // return true if the number of in flight read requests exceeds this limit
    std::uint32_t maxReadRequestsOutstanding = 100000;
    mutable std::atomic_uint32_t numReadRequestsOutstanding_ = 0;

    // mutex and condition_variable to limit the number of concurrent in flight
    // write requests
    mutable std::mutex throttleMutex_;
    mutable std::condition_variable throttleCv_;

    // writes are asynchronous. This mutex and condition_variable is used to
    // wait for all writes to finish
    mutable std::mutex syncMutex_;
    mutable std::condition_variable syncCv_;

    // io_context for read/write retries
    mutable boost::asio::io_context ioContext_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    clio::Config config_;
    uint32_t ttl_ = 0;

    mutable std::uint32_t ledgerSequence_ = 0;

public:
    CassandraBackend(
        boost::asio::io_context& ioc,
        clio::Config const& config,
        uint32_t ttl)
        : BackendInterface(config), config_(config), ttl_(ttl)
    {
        work_.emplace(ioContext_);
        ioThread_ = std::thread([this]() { ioContext_.run(); });
    }

    ~CassandraBackend() override
    {
        work_.reset();
        ioThread_.join();

        if (open_)
            close();
    }

    boost::asio::io_context&
    getIOContext() const
    {
        return ioContext_;
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
        open_ = false;
    }

    TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override;

    bool
    doFinishWritesSync()
    {
        assert(syncInterval_ == 1);
        // wait for all other writes to finish
        sync();
        // write range
        if (!range)
        {
            CassandraStatement statement{updateLedgerRange_};
            statement.bindNextInt(ledgerSequence_);
            statement.bindNextBoolean(false);
            statement.bindNextInt(ledgerSequence_);
            executeSyncWrite(statement);
        }
        CassandraStatement statement{updateLedgerRange_};
        statement.bindNextInt(ledgerSequence_);
        statement.bindNextBoolean(true);
        statement.bindNextInt(ledgerSequence_ - 1);
        if (!executeSyncUpdate(statement))
        {
            log_.warn() << "Update failed for ledger "
                        << std::to_string(ledgerSequence_) << ". Returning";
            return false;
        }
        log_.info() << "Committed ledger " << std::to_string(ledgerSequence_);
        return true;
    }

    bool
    doFinishWritesAsync()
    {
        assert(syncInterval_ != 1);
        // if db is empty, sync. if sync interval is 1, always sync.
        // if we've never synced, sync. if its been greater than the configured
        // sync interval since we last synced, sync.
        if (!range || lastSync_ == 0 ||
            ledgerSequence_ - syncInterval_ >= lastSync_)
        {
            // wait for all other writes to finish
            sync();
            // write range
            if (!range)
            {
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
            if (!executeSyncUpdate(statement))
            {
                log_.warn() << "Update failed for ledger "
                            << std::to_string(ledgerSequence_) << ". Returning";
                return false;
            }
            log_.info() << "Committed ledger "
                        << std::to_string(ledgerSequence_);
            lastSync_ = ledgerSequence_;
        }
        else
        {
            log_.info() << "Skipping commit. sync interval is "
                        << std::to_string(syncInterval_) << " - last sync is "
                        << std::to_string(lastSync_) << " - ledger sequence is "
                        << std::to_string(ledgerSequence_);
        }
        return true;
    }

    bool
    doFinishWrites() override
    {
        if (syncInterval_ == 1)
            return doFinishWritesSync();
        else
            return doFinishWritesAsync();
    }
    void
    writeLedger(ripple::LedgerInfo const& ledgerInfo, std::string&& header)
        override;

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override
    {
        // log_.trace() << "called";
        CassandraStatement statement{selectLatestLedger_};
        CassandraResult result = executeAsyncRead(statement, yield);
        if (!result.hasResult())
        {
            log_.error()
                << "CassandraBackend::fetchLatestLedgerSequence - no rows";
            return {};
        }
        return result.getUInt32();
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override
    {
        // log_.trace() << "called";
        CassandraStatement statement{selectLedgerBySeq_};
        statement.bindNextInt(sequence);
        CassandraResult result = executeAsyncRead(statement, yield);
        if (!result)
        {
            log_.error() << "No rows";
            return {};
        }
        std::vector<unsigned char> header = result.getBytes();
        return deserializeHeader(ripple::makeSlice(header));
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override
    {
        CassandraStatement statement{selectLedgerByHash_};

        statement.bindNextBytes(hash);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result.hasResult())
        {
            log_.debug() << "No rows returned";
            return {};
        }

        std::uint32_t const sequence = result.getInt64();

        return fetchLedgerBySequence(sequence, yield);
    }

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::optional<NFT>
    fetchNFT(
        ripple::uint256 const& tokenID,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t const limit,
        bool const forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context& yield) const override;

    // Synchronously fetch the object with key key, as of ledger with sequence
    // sequence
    std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override;

    std::optional<int64_t>
    getToken(void const* key, boost::asio::yield_context& yield) const
    {
        // log_.trace() << "Fetching from cassandra";
        CassandraStatement statement{getToken_};
        statement.bindNextBytes(key, 32);

        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result)
        {
            log_.error() << "No rows";
            return {};
        }
        int64_t token = result.getInt64();
        if (token == INT64_MAX)
            return {};
        else
            return token + 1;
    }

    std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override
    {
        // log_.trace() << "called";
        CassandraStatement statement{selectTransaction_};
        statement.bindNextBytes(hash);
        CassandraResult result = executeAsyncRead(statement, yield);

        if (!result)
        {
            log_.error() << "No rows";
            return {};
        }
        return {
            {result.getBytes(),
             result.getBytes(),
             result.getUInt32(),
             result.getUInt32()}};
    }

    std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    void
    doWriteLedgerObject(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& blob) override;

    void
    writeSuccessor(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& successor) override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) override;

    void
    writeNFTTransactions(std::vector<NFTTransactionsData>&& data) override;

    void
    writeTransaction(
        std::string&& hash,
        std::uint32_t const seq,
        std::uint32_t const date,
        std::string&& transaction,
        std::string&& metadata) override;

    void
    writeNFTs(std::vector<NFTsData>&& data) override;

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
    doOnlineDelete(
        std::uint32_t const numLedgersToKeep,
        boost::asio::yield_context& yield) const override;

    bool
    isTooBusy() const override;

    inline void
    incrementOutstandingRequestCount() const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!canAddRequest())
            {
                log_.debug() << "Max outstanding requests reached. "
                             << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() { return canAddRequest(); });
            }
        }
        ++numWriteRequestsOutstanding_;
    }

    inline void
    decrementOutstandingRequestCount() const
    {
        // sanity check
        if (numWriteRequestsOutstanding_ == 0)
        {
            assert(false);
            throw std::runtime_error("decrementing num outstanding below 0");
        }
        size_t cur = (--numWriteRequestsOutstanding_);
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
        return numWriteRequestsOutstanding_ < maxWriteRequestsOutstanding;
    }

    inline bool
    finishedAllRequests() const
    {
        return numWriteRequestsOutstanding_ == 0;
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
            incrementOutstandingRequestCount();
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
                log_.warn() << ss.str();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } while (rc != CASS_OK);
        cass_future_free(fut);
    }

    bool
    executeSyncUpdate(CassandraStatement const& statement) const
    {
        bool timedOut = false;
        CassFuture* fut;
        CassError rc;
        do
        {
            fut = cass_session_execute(session_.get(), statement.get());
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                timedOut = true;
                std::stringstream ss;
                ss << "Cassandra sync update error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                log_.warn() << ss.str();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } while (rc != CASS_OK);
        CassResult const* res = cass_future_get_result(fut);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            log_.error() << "executeSyncUpdate - no rows";
            cass_result_free(res);
            return false;
        }
        cass_bool_t success;
        rc = cass_value_get_bool(cass_row_get_column(row, 0), &success);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            log_.error() << "executeSyncUpdate - error getting result " << rc
                         << ", " << cass_error_desc(rc);
            return false;
        }
        cass_result_free(res);
        if (success != cass_true && timedOut)
        {
            log_.warn() << "Update failed, but timedOut is true";
            // if there was a timeout, the update may have succeeded in the
            // background on the first attempt. To determine if this happened,
            // we query the range from the db, making sure the range is what
            // we wrote. There's a possibility that another writer actually
            // applied the update, but there is no way to detect if that
            // happened. So, we just return true as long as what we tried to
            // write was what ended up being written.
            auto rng = hardFetchLedgerRangeNoThrow();
            return rng && rng->maxSequence == ledgerSequence_;
        }
        return success == cass_true;
    }

    CassandraResult
    executeAsyncRead(
        CassandraStatement const& statement,
        boost::asio::yield_context& yield) const
    {
        using result = boost::asio::async_result<
            boost::asio::yield_context,
            void(boost::system::error_code, CassError)>;

        CassFuture* fut;
        CassError rc;
        do
        {
            ++numReadRequestsOutstanding_;
            fut = cass_session_execute(session_.get(), statement.get());

            boost::system::error_code ec;
            rc = cass_future_error_code(fut, yield[ec]);
            --numReadRequestsOutstanding_;

            if (ec)
            {
                log_.error() << "Cannot read async cass_future_error_code";
            }
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra executeAsyncRead error";
                ss << ": " << cass_error_desc(rc);
                log_.error() << ss.str();
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

        // The future should have returned at the earlier cass_future_error_code
        // so we can use the sync version of this function.
        CassResult const* res = cass_future_get_result(fut);
        cass_future_free(fut);
        return {res};
    }

    CassSession*
    cautionGetSession() const
    {
        return session_.get();
    }

    std::string
    tablePrefix() const
    {
        return config_.valueOr<std::string>("table_prefix", "");
    }
};

}  // namespace Backend
