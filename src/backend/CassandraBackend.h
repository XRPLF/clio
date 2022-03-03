#ifndef RIPPLE_APP_REPORTING_CASSANDRABACKEND_H_INCLUDED
#define RIPPLE_APP_REPORTING_CASSANDRABACKEND_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <atomic>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <cassandra.h>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace Backend {

class CassandraPreparedStatement
{
private:
    CassPrepared const* prepared_ = nullptr;

public:
    CassPrepared const*
    get() const;

    bool
    prepareStatement(std::stringstream const& query, CassSession* session);

    bool
    prepareStatement(std::string const& query, CassSession* session);

    bool
    prepareStatement(char const* query, CassSession* session);

    ~CassandraPreparedStatement();
};

class CassandraStatement
{
    CassStatement* statement_ = nullptr;
    size_t curBindingIndex_ = 0;

public:
    CassandraStatement(CassandraPreparedStatement const& prepared);

    CassandraStatement(CassandraStatement&& other);

    CassandraStatement(CassandraStatement const& other) = delete;

    CassStatement*
    get() const;

    void
    bindNextBoolean(bool val);

    void
    bindNextBytes(const char* data, std::uint32_t size);

    void
    bindNextBytes(ripple::uint256 const& data);

    void
    bindNextBytes(std::vector<unsigned char> const& data);

    void
    bindNextBytes(ripple::AccountID const& data);

    void
    bindNextBytes(std::string const& data);

    void
    bindNextBytes(void const* key, std::uint32_t size);

    void
    bindNextBytes(const unsigned char* data, std::uint32_t size);

    void
    bindNextUInt(std::uint32_t value);

    void
    bindNextInt(std::uint32_t value);

    void
    bindNextInt(int64_t value);

    void
    bindNextIntTuple(std::uint32_t first, std::uint32_t second);

    ~CassandraStatement();
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

    CassandraResult(CassResult const* result);

    bool
    isOk();

    bool
    hasResult();

    bool
    operator!();

    size_t
    numRows();

    bool
    nextRow();

    std::vector<unsigned char>
    getBytes();

    ripple::uint256
    getUInt256();

    int64_t
    getInt64();

    std::uint32_t
    getUInt32();

    std::pair<std::int64_t, std::int64_t>
    getInt64Tuple();

    std::pair<Blob, Blob>
    getBytesTuple();

    ~CassandraResult();
};
inline bool
isTimeout(CassError rc);

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
    makeStatement(char const* query, std::size_t params);

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

    // maximum number of concurrent in flight requests. New requests will wait
    // for earlier requests to finish if this limit is exceeded
    std::uint32_t maxRequestsOutstanding = 10000;
    // we keep this small because the indexer runs in the background, and we
    // don't want the database to be swamped when the indexer is running
    std::uint32_t indexerMaxRequestsOutstanding = 10;
    mutable std::atomic_uint32_t numRequestsOutstanding_ = 0;

    // mutex and condition_variable to limit the number of concurrent in flight
    // requests
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

    boost::json::object config_;

    mutable std::uint32_t ledgerSequence_ = 0;

public:
    CassandraBackend(
        boost::asio::io_context& ioc,
        boost::json::object const& config);

    ~CassandraBackend() override;

    boost::asio::io_context&
    getIOContext() const;

    bool
    isOpen();

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

    AccountTransactions
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<AccountTransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override;

    bool
    doFinishWrites() override;

    void
    writeLedger(ripple::LedgerInfo const& ledgerInfo, std::string&& header)
        override;

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        std::uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        std::uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        std::uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    // Synchronously fetch the object with key key, as of ledger with sequence
    // sequence
    std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    std::optional<int64_t>
    getToken(void const* key, boost::asio::yield_context& yield) const;

    std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    void
    doWriteLedgerObject(
        std::string&& key,
        std::uint32_t seq,
        std::string&& blob) override;

    void
    writeSuccessor(
        std::string&& key,
        std::uint32_t seq,
        std::string&& successor) override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) override;

    void
    writeTransaction(
        std::string&& hash,
        std::uint32_t seq,
        std::uint32_t date,
        std::string&& transaction,
        std::string&& metadata) override;

    void
    startWrites() const override;

    void
    sync() const;

    bool
    doOnlineDelete(
        std::uint32_t numLedgersToKeep,
        boost::asio::yield_context& yield) const override;

    inline void
    incrementOutstandingRequestCount() const;

    inline void
    decrementOutstandingRequestCount() const;

    inline bool
    canAddRequest() const;

    inline bool
    finishedAllRequests() const;

    void
    finishAsyncWrite() const;

    template <class T, class S>
    void
    executeAsyncHelper(
        CassandraStatement const& statement,
        T callback,
        S& callbackData) const;

    template <class T, class S>
    void
    executeAsyncWrite(
        CassandraStatement const& statement,
        T callback,
        S& callbackData,
        bool isRetry) const;

    template <class T, class S>
    void
    executeAsyncRead(
        CassandraStatement const& statement,
        T callback,
        S& callbackData) const;

    void
    executeSyncWrite(CassandraStatement const& statement) const;

    bool
    executeSyncUpdate(CassandraStatement const& statement) const;

    CassandraResult
    executeAsyncRead(
        CassandraStatement const& statement,
        boost::asio::yield_context& yield) const;
};

}  // namespace Backend
#endif
