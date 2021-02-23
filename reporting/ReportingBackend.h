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

#ifndef RIPPLE_APP_REPORTING_REPORTINGBACKEND_H_INCLUDED
#define RIPPLE_APP_REPORTING_REPORTINGBACKEND_H_INCLUDED

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

class CassandraFlatMapBackend : public BackendInterface
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
    const CassPrepared* insertObject_ = nullptr;
    const CassPrepared* insertTransaction_ = nullptr;
    const CassPrepared* selectTransaction_ = nullptr;
    const CassPrepared* selectObject_ = nullptr;
    const CassPrepared* upperBound_ = nullptr;
    const CassPrepared* upperBound2_ = nullptr;
    const CassPrepared* getToken_ = nullptr;
    const CassPrepared* insertKey_ = nullptr;
    const CassPrepared* getCreated_ = nullptr;
    const CassPrepared* getBook_ = nullptr;
    const CassPrepared* insertBook_ = nullptr;
    const CassPrepared* deleteBook_ = nullptr;
    const CassPrepared* insertAccountTx_ = nullptr;
    const CassPrepared* selectAccountTx_ = nullptr;
    const CassPrepared* insertLedgerHeader_ = nullptr;
    const CassPrepared* insertLedgerHash_ = nullptr;
    const CassPrepared* updateLedgerRange_ = nullptr;
    const CassPrepared* updateLedgerHeader_ = nullptr;
    const CassPrepared* selectLedgerBySeq_ = nullptr;
    const CassPrepared* selectLatestLedger_ = nullptr;

    // io_context used for exponential backoff for write retries
    mutable boost::asio::io_context ioContext_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    // maximum number of concurrent in flight requests. New requests will wait
    // for earlier requests to finish if this limit is exceeded
    uint32_t maxRequestsOutstanding = 10000000;
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

public:
    CassandraFlatMapBackend(boost::json::object const& config) : config_(config)
    {
    }

    ~CassandraFlatMapBackend() override
    {
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
    open() override;

    // Close the connection to the database
    void
    close() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (insertTransaction_)
            {
                cass_prepared_free(insertTransaction_);
                insertTransaction_ = nullptr;
            }
            if (insertObject_)
            {
                cass_prepared_free(insertObject_);
                insertObject_ = nullptr;
            }
            if (insertKey_)
            {
                cass_prepared_free(insertKey_);
                insertKey_ = nullptr;
            }
            if (selectTransaction_)
            {
                cass_prepared_free(selectTransaction_);
                selectTransaction_ = nullptr;
            }
            if (selectObject_)
            {
                cass_prepared_free(selectObject_);
                selectObject_ = nullptr;
            }
            if (upperBound_)
            {
                cass_prepared_free(upperBound_);
                upperBound_ = nullptr;
            }
            if (getToken_)
            {
                cass_prepared_free(getToken_);
                getToken_ = nullptr;
            }
            if (getCreated_)
            {
                cass_prepared_free(getCreated_);
                getCreated_ = nullptr;
            }
            if (getBook_)
            {
                cass_prepared_free(getBook_);
                getBook_ = nullptr;
            }
            if (insertBook_)
            {
                cass_prepared_free(insertBook_);
                insertBook_ = nullptr;
            }
            if (deleteBook_)
            {
                cass_prepared_free(deleteBook_);
                deleteBook_ = nullptr;
            }
            if (insertAccountTx_)
            {
                cass_prepared_free(insertAccountTx_);
                insertAccountTx_ = nullptr;
            }

            if (selectAccountTx_)
            {
                cass_prepared_free(selectAccountTx_);
                selectAccountTx_ = nullptr;
            }
            if (insertLedgerHeader_)
            {
                cass_prepared_free(insertLedgerHeader_);
                insertLedgerHeader_ = nullptr;
            }
            if (insertLedgerHash_)
            {
                cass_prepared_free(insertLedgerHash_);
                insertLedgerHash_ = nullptr;
            }
            if (updateLedgerRange_)
            {
                cass_prepared_free(updateLedgerRange_);
                updateLedgerRange_ = nullptr;
            }
            work_.reset();
            ioThread_.join();
        }
        open_ = false;
    }

    std::pair<
        std::vector<BackendInterface::TransactionAndMetadata>,
        std::optional<BackendInterface::AccountTransactionsCursor>>
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::optional<BackendInterface::AccountTransactionsCursor> const&
            cursor) const override
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doAccountTx";
        CassStatement* statement = cass_prepared_bind(selectAccountTx_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);

        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(account.data()), 20);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra account_tx query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }
        CassTuple* cassCursor = cass_tuple_new(2);
        if (cursor)
        {
            cass_tuple_set_int64(cassCursor, 0, cursor->ledgerSequence);
            cass_tuple_set_int64(cassCursor, 1, cursor->transactionIndex);
        }
        else
        {
            cass_tuple_set_int64(cassCursor, 0, INT32_MAX);
            cass_tuple_set_int64(cassCursor, 1, INT32_MAX);
        }
        rc = cass_statement_bind_tuple(statement, 1, cassCursor);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra tuple to account_tx: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }

        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra account_tx fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        BOOST_LOG_TRIVIAL(debug) << "doAccountTx - got hashes";
        std::vector<ripple::uint256> hashes;
        size_t numRows = cass_result_row_count(res);
        bool more = numRows == 300;

        CassIterator* iter = cass_iterator_from_result(res);
        std::optional<AccountTransactionsCursor> retCursor;
        while (cass_iterator_next(iter))
        {
            CassRow const* row = cass_iterator_get_row(iter);

            cass_byte_t const* outData;
            std::size_t outSize;

            CassValue const* hash = cass_row_get_column(row, 0);
            rc = cass_value_get_bytes(hash, &outData, &outSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(iter);

                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            hashes.push_back(ripple::uint256::fromVoid(outData));
            --numRows;
            if (numRows == 0)
            {
                if (more)
                {
                    CassValue const* cassCursorVal =
                        cass_row_get_column(row, 1);
                    CassIterator* tupleIter =
                        cass_iterator_from_tuple(cassCursorVal);
                    cass_iterator_next(tupleIter);
                    CassValue const* seqVal =
                        cass_iterator_get_value(tupleIter);
                    cass_iterator_next(tupleIter);
                    CassValue const* idxVal =
                        cass_iterator_get_value(tupleIter);
                    int64_t seqOut;
                    int64_t idxOut;
                    cass_value_get_int64(seqVal, &seqOut);
                    cass_value_get_int64(idxVal, &idxOut);
                    retCursor = {(uint32_t)seqOut, (uint32_t)idxOut};
                }
            }
        }
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
        CassandraFlatMapBackend const* backend;
        uint32_t sequence;
        std::string header;
        uint32_t currentRetries = 0;

        WriteLedgerHeaderCallbackData(
            CassandraFlatMapBackend const* f,
            uint32_t sequence,
            std::string&& header)
            : backend(f), sequence(sequence), header(std::move(header))
        {
        }
    };
    struct WriteLedgerHashCallbackData
    {
        CassandraFlatMapBackend const* backend;
        ripple::uint256 hash;
        uint32_t sequence;
        uint32_t currentRetries = 0;

        WriteLedgerHashCallbackData(
            CassandraFlatMapBackend const* f,
            ripple::uint256 hash,
            uint32_t sequence)
            : backend(f), hash(hash), sequence(sequence)
        {
        }
    };
    bool
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
        ++numRequestsOutstanding_;
        ++numRequestsOutstanding_;
        writeLedgerHeader(*headerCb, false);
        writeLedgerHash(*hashCb, false);
        // wait for all other writes to finish
        sync();
        // write range
        if (isFirst)
        {
            CassStatement* statement = cass_prepared_bind(updateLedgerRange_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            CassError rc =
                cass_statement_bind_int64(statement, 0, ledgerInfo.seq);
            rc = cass_statement_bind_bool(statement, 1, cass_false);

            rc = cass_statement_bind_int64(statement, 2, ledgerInfo.seq);
            CassFuture* fut;
            do
            {
                fut = cass_session_execute(session_.get(), statement);
                rc = cass_future_error_code(fut);
                if (rc != CASS_OK)
                {
                    std::stringstream ss;
                    ss << "Cassandra write error";
                    ss << ", retrying";
                    ss << ": " << cass_error_desc(rc);
                    BOOST_LOG_TRIVIAL(warning) << ss.str();
                }
            } while (rc != CASS_OK);
            cass_statement_free(statement);
        }
        CassStatement* statement = cass_prepared_bind(updateLedgerRange_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        // TODO check rc
        CassError rc = cass_statement_bind_int64(statement, 0, ledgerInfo.seq);
        assert(rc == CASS_OK);
        rc = cass_statement_bind_bool(statement, 1, cass_true);
        assert(rc == CASS_OK);
        rc = cass_statement_bind_int64(statement, 2, ledgerInfo.seq);
        assert(rc == CASS_OK);
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra write error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);
        cass_statement_free(statement);
        CassResult const* res = cass_future_get_result(fut);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra write error: no rows";
            cass_result_free(res);
            return false;
        }
        cass_bool_t success;
        rc = cass_value_get_bool(cass_row_get_column(row, 0), &success);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra write error: " << rc << ", "
                                     << cass_error_desc(rc);
            return false;
        }
        cass_result_free(res);
        return success == cass_true;
    }
    void
    writeLedgerHash(WriteLedgerHashCallbackData& cb, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        CassStatement* statement = cass_prepared_bind(insertLedgerHash_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(cb.hash.data()), 32);

        assert(rc == CASS_OK);
        rc = cass_statement_bind_int64(statement, 1, cb.sequence);
        assert(rc == CASS_OK);
        // actually do the write
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteLedgerHashCallback, static_cast<void*>(&cb));
        cass_future_free(fut);
    }

    void
    writeLedgerHeader(WriteLedgerHeaderCallbackData& cb, bool isRetry) const
    {
        // write header
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        unsigned char* headerRaw = (unsigned char*)cb.header.data();
        CassStatement* statement = cass_prepared_bind(insertLedgerHeader_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_int64(statement, 0, cb.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra insert ledger header: " << rc << ", "
                << cass_error_desc(rc);
            return;
        }
        rc = cass_statement_bind_bytes(
            statement,
            1,
            static_cast<cass_byte_t const*>(headerRaw),
            cb.header.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra insert ledger header: " << rc << ", "
                << cass_error_desc(rc);
            return;
        }
        // actually do the write
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteLedgerHeaderCallback, static_cast<void*>(&cb));
        cass_future_free(fut);
    }

    std::optional<uint32_t>
    fetchLatestLedgerSequence() const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectLatestLedger_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassFuture* fut;
        CassError rc;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_int64_t sequence;
        rc = cass_value_get_int64(cass_row_get_column(row, 0), &sequence);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return sequence;
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectLedgerBySeq_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_int64(statement, 0, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra ledger fetch query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_byte_t const* buf;
        std::size_t bufSize;
        rc = cass_value_get_bytes(cass_row_get_column(row, 0), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> result{buf, buf + bufSize};
        ripple::LedgerInfo lgrInfo =
            deserializeHeader(ripple::makeSlice(result));
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return lgrInfo;
    }

    // Synchronously fetch the object with key key and store the result in
    // pno
    // @param key the key of the object
    // @param pno object in which to store the result
    // @return result status of query
    std::optional<BackendInterface::Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence)
        const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectObject_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(key.data()), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 1, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_byte_t const* buf;
        std::size_t bufSize;
        rc = cass_value_get_bytes(cass_row_get_column(row, 0), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> result{buf, buf + bufSize};
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return result;
    }

    std::optional<int64_t>
    getToken(void const* key) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(getToken_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(key), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_int64_t token;
        rc = cass_value_get_int64(cass_row_get_column(row, 0), &token);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        cass_result_free(res);
        if (token == INT64_MAX)
            return {};
        return token + 1;
    }

    std::optional<BackendInterface::TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(hash.data()), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_byte_t const* txBuf;
        std::size_t txBufSize;
        rc = cass_value_get_bytes(
            cass_row_get_column(row, 0), &txBuf, &txBufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> txResult{txBuf, txBuf + txBufSize};
        cass_byte_t const* metaBuf;
        std::size_t metaBufSize;
        rc = cass_value_get_bytes(
            cass_row_get_column(row, 0), &metaBuf, &metaBufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> metaResult{metaBuf, metaBuf + metaBufSize};
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return {{txResult, metaResult}};
    }
    BackendInterface::LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const override
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doUpperBound";
        CassStatement* statement = cass_prepared_bind(upperBound_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);

        int64_t intCursor = INT64_MIN;
        if (cursor)
        {
            auto token = getToken(cursor->data());
            if (token)
                intCursor = *token;
        }

        CassError rc = cass_statement_bind_int64(statement, 0, intCursor);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra hash to doUpperBound query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int64(statement, 1, ledgerSequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra seq to doUpperBound query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 2, ledgerSequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra seq to doUpperBound query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int32(statement, 3, limit);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra limit to doUpperBound query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        BOOST_LOG_TRIVIAL(debug) << "doUpperBound - got keys";
        std::vector<ripple::uint256> keys;

        CassIterator* iter = cass_iterator_from_result(res);
        while (cass_iterator_next(iter))
        {
            CassRow const* row = cass_iterator_get_row(iter);

            cass_byte_t const* outData;
            std::size_t outSize;

            CassValue const* hash = cass_row_get_column(row, 0);
            rc = cass_value_get_bytes(hash, &outData, &outSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(iter);

                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            keys.push_back(ripple::uint256::fromVoid(outData));
        }
        BOOST_LOG_TRIVIAL(debug)
            << "doUpperBound - populated keys. num keys = " << keys.size();
        if (keys.size())
        {
            std::vector<LedgerObject> results;
            std::vector<BackendInterface::Blob> objs =
                fetchLedgerObjects(keys, ledgerSequence);
            for (size_t i = 0; i < objs.size(); ++i)
            {
                results.push_back({keys[i], objs[i]});
            }
            return {results, keys[keys.size() - 1]};
        }

        return {{}, {}};
    }

    std::vector<BackendInterface::LedgerObject>
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t sequence,
        std::optional<ripple::uint256> const& cursor) const override
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doBookOffers";
        CassStatement* statement = cass_prepared_bind(getBook_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(book.data()), 32);

        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra book to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int64(statement, 1, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra sequence to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 2, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra deleted_at to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }
        if (cursor)
            rc = cass_statement_bind_bytes(
                statement,
                3,
                static_cast<cass_byte_t const*>(cursor->data()),
                32);
        else
        {
            ripple::uint256 zero = {};
            rc = cass_statement_bind_bytes(
                statement, 3, static_cast<cass_byte_t const*>(zero.data()), 32);
        }

        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra book to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        BOOST_LOG_TRIVIAL(debug) << "doUpperBound - got keys";
        std::vector<ripple::uint256> keys;

        CassIterator* iter = cass_iterator_from_result(res);
        while (cass_iterator_next(iter))
        {
            CassRow const* row = cass_iterator_get_row(iter);

            cass_byte_t const* outData;
            std::size_t outSize;

            CassValue const* hash = cass_row_get_column(row, 0);
            rc = cass_value_get_bytes(hash, &outData, &outSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(iter);

                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            keys.push_back(ripple::uint256::fromVoid(outData));
            std::cout << ripple::strHex(keys.back()) << std::endl;
        }
        BOOST_LOG_TRIVIAL(debug)
            << "doBookOffers - populated keys. num keys = " << keys.size();
        if (keys.size())
        {
            std::vector<LedgerObject> results;
            std::vector<BackendInterface::Blob> objs =
                fetchLedgerObjects(keys, sequence);
            for (size_t i = 0; i < objs.size(); ++i)
            {
                results.push_back({keys[i], objs[i]});
            }
            return results;
        }

        return {};
    }
    bool
    canFetchBatch()
    {
        return true;
    }

    struct ReadCallbackData
    {
        CassandraFlatMapBackend const& backend;
        ripple::uint256 const& hash;
        BackendInterface::TransactionAndMetadata& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadCallbackData(
            CassandraFlatMapBackend const& backend,
            ripple::uint256 const& hash,
            BackendInterface::TransactionAndMetadata& result,
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

    std::vector<BackendInterface::TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes) const override
    {
        std::size_t const numHashes = hashes.size();
        BOOST_LOG_TRIVIAL(trace)
            << "Fetching " << numHashes << " records from Cassandra";
        std::atomic_uint32_t numFinished = 0;
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<BackendInterface::TransactionAndMetadata> results{
            numHashes};
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

        BOOST_LOG_TRIVIAL(trace)
            << "Fetched " << numHashes << " records from Cassandra";
        return results;
    }

    void
    read(ReadCallbackData& data) const
    {
        CassStatement* statement = cass_prepared_bind(selectTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(data.hash.data()),
            32);
        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }

        CassFuture* fut = cass_session_execute(session_.get(), statement);

        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapReadCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    struct ReadObjectCallbackData
    {
        CassandraFlatMapBackend const& backend;
        ripple::uint256 const& key;
        uint32_t sequence;
        BackendInterface::Blob& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadObjectCallbackData(
            CassandraFlatMapBackend const& backend,
            ripple::uint256 const& key,
            uint32_t sequence,
            BackendInterface::Blob& result,
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
    std::vector<BackendInterface::Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const override
    {
        std::size_t const numKeys = keys.size();
        BOOST_LOG_TRIVIAL(trace)
            << "Fetching " << numKeys << " records from Cassandra";
        std::atomic_uint32_t numFinished = 0;
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<BackendInterface::Blob> results{numKeys};
        std::vector<std::shared_ptr<ReadObjectCallbackData>> cbs;
        cbs.reserve(numKeys);
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            cbs.push_back(std::make_shared<ReadObjectCallbackData>(
                *this,
                keys[i],
                sequence,
                results[i],
                cv,
                numFinished,
                numKeys));
            readObject(*cbs[i]);
        }
        assert(results.size() == cbs.size());

        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(
            lck, [&numFinished, &numKeys]() { return numFinished == numKeys; });

        BOOST_LOG_TRIVIAL(trace)
            << "Fetched " << numKeys << " records from Cassandra";
        return results;
    }
    void
    readObject(ReadObjectCallbackData& data) const
    {
        CassStatement* statement = cass_prepared_bind(selectObject_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(data.key.data()), 32);
        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }
        rc = cass_statement_bind_int64(statement, 1, data.sequence);

        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }

        CassFuture* fut = cass_session_execute(session_.get(), statement);

        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapReadObjectCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    struct WriteCallbackData
    {
        CassandraFlatMapBackend const* backend;
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
            CassandraFlatMapBackend const* f,
            std::string&& key,
            uint32_t sequence,
            std::string&& blob,
            bool isCreated,
            bool isDeleted,
            std::optional<ripple::uint256>&& book)
            : backend(f)
            , key(std::move(key))
            , sequence(sequence)
            , blob(std::move(blob))
            , isCreated(isCreated)
            , isDeleted(isDeleted)
            , book(std::move(book))
        {
            if (isCreated or isDeleted)
                ++refs;
            if (book)
                ++refs;
        }
    };
    struct WriteAccountTxCallbackData
    {
        CassandraFlatMapBackend const* backend;
        AccountTransactionsData data;

        uint32_t currentRetries = 0;
        std::atomic<int> refs;

        WriteAccountTxCallbackData(
            CassandraFlatMapBackend const* f,
            AccountTransactionsData&& data)
            : backend(f), data(std::move(data)), refs(data.accounts.size())
        {
        }
    };

    void
    write(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        {
            CassStatement* statement = cass_prepared_bind(insertObject_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 1, data.sequence);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            const unsigned char* blobData = (unsigned char*)data.blob.data();
            rc = cass_statement_bind_bytes(
                statement,
                2,
                static_cast<cass_byte_t const*>(blobData),
                data.blob.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapWriteCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
    }

    void
    writeDeletedKey(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        CassStatement* statement = cass_prepared_bind(insertKey_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* keyData = (unsigned char*)data.key.data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(keyData),
            data.key.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 1, data.createdSequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 2, data.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteKeyCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    void
    writeKey(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        if (data.isCreated)
        {
            CassStatement* statement = cass_prepared_bind(insertKey_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 1, data.sequence);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 2, INT64_MAX);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapWriteKeyCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
        else if (data.isDeleted)
        {
            CassStatement* statement = cass_prepared_bind(getCreated_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapGetCreatedCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
    }

    void
    writeBook(WriteCallbackData& data, bool isRetry) const
    {
        assert(data.isCreated or data.isDeleted);
        if (!data.isCreated and !data.isDeleted)
            throw std::runtime_error(
                "writing book that's neither created or deleted");
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        CassStatement* statement = nullptr;
        if (data.isCreated)
            statement = cass_prepared_bind(insertBook_);
        else if (data.isDeleted)
            statement = cass_prepared_bind(deleteBook_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* bookData = (unsigned char*)data.book->data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(bookData),
            data.book->size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* keyData = (unsigned char*)data.key.data();
        rc = cass_statement_bind_bytes(
            statement,
            1,
            static_cast<cass_byte_t const*>(keyData),
            data.key.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 2, data.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        if (data.isCreated)
        {
            rc = cass_statement_bind_int64(statement, 3, INT64_MAX);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;

                ss << "binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteBookCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }
    void
    writeLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const override
    {
        BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
        WriteCallbackData* data = new WriteCallbackData(
            this,
            std::move(key),
            seq,
            std::move(blob),
            isCreated,
            isDeleted,
            std::move(book));

        ++numRequestsOutstanding_;
        if (isCreated || isDeleted)
            ++numRequestsOutstanding_;
        if (book)
            ++numRequestsOutstanding_;
        write(*data, false);
        if (isCreated || isDeleted)
            writeKey(*data, false);
        if (book)
            writeBook(*data, false);
        // handle book
    }

    void
    writeAccountTransactions(AccountTransactionsData&& data) const override
    {
        numRequestsOutstanding_ += data.accounts.size();
        WriteAccountTxCallbackData* cbData =
            new WriteAccountTxCallbackData(this, std::move(data));
        writeAccountTx(*cbData, false);
    }

    void
    writeAccountTx(WriteAccountTxCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        for (auto const& account : data.data.accounts)
        {
            CassStatement* statement = cass_prepared_bind(insertAccountTx_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* accountData = (unsigned char*)account.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(accountData),
                account.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert account: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassTuple* idx = cass_tuple_new(2);
            rc = cass_tuple_set_int64(idx, 0, data.data.ledgerSequence);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding ledger sequence to tuple: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_tuple_set_int64(idx, 1, data.data.transactionIndex);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding tx idx to tuple: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_tuple(statement, 1, idx);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding tuple: " << rc << ", " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapWriteAccountTxCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
    }

    struct WriteTransactionCallbackData
    {
        CassandraFlatMapBackend const* backend;
        // The shared pointer to the node object must exist until it's
        // confirmed persisted. Otherwise, it can become deleted
        // prematurely if other copies are removed from caches.
        std::string hash;
        uint32_t sequence;
        std::string transaction;
        std::string metadata;

        uint32_t currentRetries = 0;

        WriteTransactionCallbackData(
            CassandraFlatMapBackend const* f,
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
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }

        CassStatement* statement = cass_prepared_bind(insertTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* hashData = (unsigned char*)data.hash.data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(hashData),
            data.hash.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 1, data.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* transactionData =
            (unsigned char*)data.transaction.data();
        rc = cass_statement_bind_bytes(
            statement,
            2,
            static_cast<cass_byte_t const*>(transactionData),
            data.transaction.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* metadata = (unsigned char*)data.metadata.data();
        rc = cass_statement_bind_bytes(
            statement,
            3,
            static_cast<cass_byte_t const*>(metadata),
            data.metadata.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteTransactionCallback, static_cast<void*>(&data));
        cass_future_free(fut);
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

        ++numRequestsOutstanding_;
        writeTransaction(*data, false);
    }

    void
    sync() const override
    {
        std::unique_lock<std::mutex> lck(syncMutex_);

        syncCv_.wait(lck, [this]() { return numRequestsOutstanding_ == 0; });
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
};

#endif
