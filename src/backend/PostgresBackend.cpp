#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <backend/PostgresBackend.h>
#include <thread>
namespace Backend {

// Type alias for async completion handlers
using completion_token = boost::asio::yield_context;
using function_type = void(boost::system::error_code);
using result_type = boost::asio::async_result<completion_token, function_type>;
using handler_type = typename result_type::completion_handler_type;

struct HandlerWrapper
{
    handler_type handler;

    HandlerWrapper(handler_type&& handler_) : handler(std::move(handler_))
    {
    }
};

PostgresBackend::PostgresBackend(
    boost::asio::io_context& ioc,
    boost::json::object const& config)
    : BackendInterface(config)
    , pgPool_(make_PgPool(ioc, config))
    , writeConnection_(pgPool_)
{
    if (config.contains("write_interval"))
    {
        writeInterval_ = config.at("write_interval").as_int64();
    }
}
void
PostgresBackend::writeLedger(
    ripple::LedgerInfo const& ledgerInfo,
    std::string&& ledgerHeader)
{
    synchronous([&](boost::asio::yield_context yield) {
        auto cmd = boost::format(
            R"(INSERT INTO ledgers
            VALUES (%u,'\x%s', '\x%s',%u,%u,%u,%u,%u,'\x%s','\x%s'))");

        auto ledgerInsert = boost::str(
            cmd % ledgerInfo.seq % ripple::strHex(ledgerInfo.hash) %
            ripple::strHex(ledgerInfo.parentHash) % ledgerInfo.drops.drops() %
            ledgerInfo.closeTime.time_since_epoch().count() %
            ledgerInfo.parentCloseTime.time_since_epoch().count() %
            ledgerInfo.closeTimeResolution.count() % ledgerInfo.closeFlags %
            ripple::strHex(ledgerInfo.accountHash) %
            ripple::strHex(ledgerInfo.txHash));

        auto res = writeConnection_(ledgerInsert.data(), yield);
        abortWrite_ = !res;
        inProcessLedger = ledgerInfo.seq;
    });
}

void
PostgresBackend::writeAccountTransactions(
    std::vector<AccountTransactionsData>&& data)
{
    if (abortWrite_)
        return;
    PgQuery pg(pgPool_);
    for (auto const& record : data)
    {
        for (auto const& a : record.accounts)
        {
            std::string acct = ripple::strHex(a);
            accountTxBuffer_ << "\\\\x" << acct << '\t'
                             << std::to_string(record.ledgerSequence) << '\t'
                             << std::to_string(record.transactionIndex) << '\t'
                             << "\\\\x" << ripple::strHex(record.txHash)
                             << '\n';
        }
    }
}

void
PostgresBackend::doWriteLedgerObject(
    std::string&& key,
    std::uint32_t const seq,
    std::string&& blob)
{
    synchronous([&](boost::asio::yield_context yield) {
        if (abortWrite_)
            return;
        objectsBuffer_ << "\\\\x" << ripple::strHex(key) << '\t'
                       << std::to_string(seq) << '\t' << "\\\\x"
                       << ripple::strHex(blob) << '\n';
        numRowsInObjectsBuffer_++;
        // If the buffer gets too large, the insert fails. Not sure why. So we
        // insert after 1 million records
        if (numRowsInObjectsBuffer_ % writeInterval_ == 0)
        {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Flushing large buffer. num objects = "
                << numRowsInObjectsBuffer_;
            writeConnection_.bulkInsert("objects", objectsBuffer_.str(), yield);
            BOOST_LOG_TRIVIAL(info) << __func__ << " Flushed large buffer";
            objectsBuffer_.str("");
        }
    });
}

void
PostgresBackend::writeSuccessor(
    std::string&& key,
    std::uint32_t const seq,
    std::string&& successor)
{
    synchronous([&](boost::asio::yield_context yield) {
        if (range)
        {
            if (successors_.count(key) > 0)
                return;
            successors_.insert(key);
        }
        successorBuffer_ << "\\\\x" << ripple::strHex(key) << '\t'
                         << std::to_string(seq) << '\t' << "\\\\x"
                         << ripple::strHex(successor) << '\n';
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << ripple::strHex(key) << " - " << std::to_string(seq);
        numRowsInSuccessorBuffer_++;
        if (numRowsInSuccessorBuffer_ % writeInterval_ == 0)
        {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " Flushing large buffer. num successors = "
                << numRowsInSuccessorBuffer_;
            writeConnection_.bulkInsert(
                "successor", successorBuffer_.str(), yield);
            BOOST_LOG_TRIVIAL(info) << __func__ << " Flushed large buffer";
            successorBuffer_.str("");
        }
    });
}

void
PostgresBackend::writeTransaction(
    std::string&& hash,
    std::uint32_t const seq,
    std::uint32_t const date,
    std::string&& transaction,
    std::string&& metadata)
{
    if (abortWrite_)
        return;
    transactionsBuffer_ << "\\\\x" << ripple::strHex(hash) << '\t'
                        << std::to_string(seq) << '\t' << std::to_string(date)
                        << '\t' << "\\\\x" << ripple::strHex(transaction)
                        << '\t' << "\\\\x" << ripple::strHex(metadata) << '\n';
}

std::uint32_t
checkResult(PgResult const& res, std::uint32_t const numFieldsExpected)
{
    if (!res)
    {
        auto msg = res.msg();
        BOOST_LOG_TRIVIAL(debug) << msg;
        if (msg.find("statement timeout"))
            throw DatabaseTimeout();
        assert(false);
        throw std::runtime_error(msg);
    }
    if (res.status() != PGRES_TUPLES_OK)
    {
        std::stringstream msg;
        msg << " : Postgres response should have been "
               "PGRES_TUPLES_OK but instead was "
            << res.status() << " - msg  = " << res.msg();
        assert(false);
        throw std::runtime_error(msg.str());
    }

    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " Postgres result msg  : " << res.msg();
    if (res.isNull() || res.ntuples() == 0)
    {
        return 0;
    }
    else if (res.ntuples() > 0)
    {
        if (res.nfields() != numFieldsExpected)
        {
            std::stringstream msg;
            msg << "Wrong number of fields in Postgres "
                   "response. Expected "
                << numFieldsExpected << ", but got " << res.nfields();
            throw std::runtime_error(msg.str());
            assert(false);
        }
    }
    return res.ntuples();
}

ripple::LedgerInfo
parseLedgerInfo(PgResult const& res)
{
    std::int64_t ledgerSeq = res.asBigInt(0, 0);
    ripple::uint256 hash = res.asUInt256(0, 1);
    ripple::uint256 prevHash = res.asUInt256(0, 2);
    std::int64_t totalCoins = res.asBigInt(0, 3);
    std::int64_t closeTime = res.asBigInt(0, 4);
    std::int64_t parentCloseTime = res.asBigInt(0, 5);
    std::int64_t closeTimeRes = res.asBigInt(0, 6);
    std::int64_t closeFlags = res.asBigInt(0, 7);
    ripple::uint256 accountHash = res.asUInt256(0, 8);
    ripple::uint256 txHash = res.asUInt256(0, 9);

    using time_point = ripple::NetClock::time_point;
    using duration = ripple::NetClock::duration;

    ripple::LedgerInfo info;
    info.seq = ledgerSeq;
    info.hash = hash;
    info.parentHash = prevHash;
    info.drops = totalCoins;
    info.closeTime = time_point{duration{closeTime}};
    info.parentCloseTime = time_point{duration{parentCloseTime}};
    info.closeFlags = closeFlags;
    info.closeTimeResolution = duration{closeTimeRes};
    info.accountHash = accountHash;
    info.txHash = txHash;
    info.validated = true;
    return info;
}
std::optional<std::uint32_t>
PostgresBackend::fetchLatestLedgerSequence(
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    auto const query =
        "SELECT ledger_seq FROM ledgers ORDER BY ledger_seq DESC LIMIT 1";

    if (auto res = pgQuery(query, yield); checkResult(res, 1))
        return res.asBigInt(0, 0);

    return {};
}

std::optional<ripple::LedgerInfo>
PostgresBackend::fetchLedgerBySequence(
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT * FROM ledgers WHERE ledger_seq = "
        << std::to_string(sequence);

    if (auto res = pgQuery(sql.str().data(), yield); checkResult(res, 10))
        return parseLedgerInfo(res);

    return {};
}

std::optional<ripple::LedgerInfo>
PostgresBackend::fetchLedgerByHash(
    ripple::uint256 const& hash,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT * FROM ledgers WHERE ledger_hash = \'\\x"
        << ripple::to_string(hash) << "\'";

    if (auto res = pgQuery(sql.str().data(), yield); checkResult(res, 10))
        return parseLedgerInfo(res);

    return {};
}

std::optional<LedgerRange>
PostgresBackend::hardFetchLedgerRange(boost::asio::yield_context& yield) const
{
    auto range = PgQuery(pgPool_)("SELECT complete_ledgers()", yield);
    if (!range)
        return {};

    std::string res{range.c_str()};
    BOOST_LOG_TRIVIAL(debug) << "range is = " << res;
    try
    {
        size_t minVal = 0;
        size_t maxVal = 0;
        if (res == "empty" || res == "error" || res.empty())
            return {};
        else if (size_t delim = res.find('-'); delim != std::string::npos)
        {
            minVal = std::stol(res.substr(0, delim));
            maxVal = std::stol(res.substr(delim + 1));
        }
        else
        {
            minVal = maxVal = std::stol(res);
        }
        return LedgerRange{minVal, maxVal};
    }
    catch (std::exception&)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : "
            << "Error parsing result of getCompleteLedgers()";
    }
    return {};
}

std::optional<Blob>
PostgresBackend::doFetchLedgerObject(
    ripple::uint256 const& key,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT object FROM objects WHERE key = "
        << "\'\\x" << ripple::strHex(key) << "\'"
        << " AND ledger_seq <= " << std::to_string(sequence)
        << " ORDER BY ledger_seq DESC LIMIT 1";

    if (auto res = pgQuery(sql.str().data(), yield); checkResult(res, 1))
    {
        auto blob = res.asUnHexedBlob(0, 0);
        if (blob.size())
            return blob;
    }

    return {};
}

// returns a transaction, metadata pair
std::optional<TransactionAndMetadata>
PostgresBackend::fetchTransaction(
    ripple::uint256 const& hash,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT transaction,metadata,ledger_seq,date FROM transactions "
           "WHERE hash = "
        << "\'\\x" << ripple::strHex(hash) << "\'";

    if (auto res = pgQuery(sql.str().data(), yield); checkResult(res, 4))
    {
        return {
            {res.asUnHexedBlob(0, 0),
             res.asUnHexedBlob(0, 1),
             res.asBigInt(0, 2),
             res.asBigInt(0, 3)}};
    }

    return {};
}
std::vector<TransactionAndMetadata>
PostgresBackend::fetchAllTransactionsInLedger(
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT transaction, metadata, ledger_seq,date FROM transactions "
           "WHERE "
        << "ledger_seq = " << std::to_string(ledgerSequence);

    auto res = pgQuery(sql.str().data(), yield);
    if (size_t numRows = checkResult(res, 4))
    {
        std::vector<TransactionAndMetadata> txns;
        for (size_t i = 0; i < numRows; ++i)
        {
            txns.push_back(
                {res.asUnHexedBlob(i, 0),
                 res.asUnHexedBlob(i, 1),
                 res.asBigInt(i, 2),
                 res.asBigInt(i, 3)});
        }
        return txns;
    }
    return {};
}
std::vector<ripple::uint256>
PostgresBackend::fetchAllTransactionHashesInLedger(
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT hash FROM transactions WHERE "
        << "ledger_seq = " << std::to_string(ledgerSequence);

    auto res = pgQuery(sql.str().data(), yield);
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<ripple::uint256> hashes;
        for (size_t i = 0; i < numRows; ++i)
        {
            hashes.push_back(res.asUInt256(i, 0));
        }
        return hashes;
    }

    return {};
}

std::optional<ripple::uint256>
PostgresBackend::doFetchSuccessorKey(
    ripple::uint256 key,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT next FROM successor WHERE key = "
        << "\'\\x" << ripple::strHex(key) << "\'"
        << " AND ledger_seq <= " << std::to_string(ledgerSequence)
        << " ORDER BY ledger_seq DESC LIMIT 1";

    if (auto res = pgQuery(sql.str().data(), yield); checkResult(res, 1))
    {
        auto next = res.asUInt256(0, 0);
        if (next == lastKey)
            return {};
        return next;
    }

    return {};
}

std::vector<TransactionAndMetadata>
PostgresBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes,
    boost::asio::yield_context& yield) const
{
    if (!hashes.size())
        return {};

    std::vector<TransactionAndMetadata> results;
    results.resize(hashes.size());

    handler_type handler(std::forward<decltype(yield)>(yield));
    result_type result(handler);

    auto hw = new HandlerWrapper(std::move(handler));

    auto start = std::chrono::system_clock::now();
    auto end = std::chrono::system_clock::now();
    auto duration = ((end - start).count()) / 1000000000.0;

    std::atomic_uint numRemaining = hashes.size();

    for (size_t i = 0; i < hashes.size(); ++i)
    {
        auto const& hash = hashes[i];
        boost::asio::spawn(
            get_associated_executor(yield),
            [this, &hash, &results, hw, &numRemaining, i](
                boost::asio::yield_context yield) {
                BOOST_LOG_TRIVIAL(trace) << __func__ << " getting txn = " << i;

                PgQuery pgQuery(pgPool_);

                std::stringstream sql;
                sql << "SELECT transaction,metadata,ledger_seq,date FROM "
                       "transactions "
                       "WHERE HASH = \'\\x"
                    << ripple::strHex(hash) << "\'";

                if (auto const res = pgQuery(sql.str().data(), yield);
                    checkResult(res, 4))
                {
                    results[i] = {
                        res.asUnHexedBlob(0, 0),
                        res.asUnHexedBlob(0, 1),
                        res.asBigInt(0, 2),
                        res.asBigInt(0, 3)};
                }

                if (--numRemaining == 0)
                {
                    handler_type h(std::move(hw->handler));
                    h(boost::system::error_code{});
                }
            });
    }

    // Yields the worker to the io_context until handler is called.
    result.get();

    delete hw;

    auto end2 = std::chrono::system_clock::now();
    duration = ((end2 - end).count()) / 1000000000.0;

    BOOST_LOG_TRIVIAL(info)
        << __func__ << " fetched " << std::to_string(hashes.size())
        << " transactions with threadpool. took " << std::to_string(duration);

    return results;
}

std::vector<Blob>
PostgresBackend::doFetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    if (!keys.size())
        return {};

    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::vector<Blob> results;
    results.resize(keys.size());

    handler_type handler(std::forward<decltype(yield)>(yield));
    result_type result(handler);

    auto hw = new HandlerWrapper(std::move(handler));

    std::atomic_uint numRemaining = keys.size();
    auto start = std::chrono::system_clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
    {
        auto const& key = keys[i];
        boost::asio::spawn(
            boost::asio::get_associated_executor(yield),
            [this, &key, &results, &numRemaining, hw, i, sequence](
                boost::asio::yield_context yield) {
                PgQuery pgQuery(pgPool_);

                std::stringstream sql;
                sql << "SELECT object FROM "
                       "objects "
                       "WHERE key = \'\\x"
                    << ripple::strHex(key) << "\'"
                    << " AND ledger_seq <= " << std::to_string(sequence)
                    << " ORDER BY ledger_seq DESC LIMIT 1";

                if (auto const res = pgQuery(sql.str().data(), yield);
                    checkResult(res, 1))
                    results[i] = res.asUnHexedBlob();

                if (--numRemaining == 0)
                {
                    handler_type h(std::move(hw->handler));
                    h(boost::system::error_code{});
                }
            });
    }

    // Yields the worker to the io_context until handler is called.
    result.get();

    delete hw;

    auto end = std::chrono::system_clock::now();
    auto duration = ((end - start).count()) / 1000000000.0;

    BOOST_LOG_TRIVIAL(info)
        << __func__ << " fetched " << std::to_string(keys.size())
        << " objects with threadpool. took " << std::to_string(duration);

    return results;
}

std::vector<LedgerObject>
PostgresBackend::fetchLedgerDiff(
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);

    std::stringstream sql;
    sql << "SELECT key,object FROM objects "
           "WHERE "
        << "ledger_seq = " << std::to_string(ledgerSequence);

    auto res = pgQuery(sql.str().data(), yield);
    if (size_t numRows = checkResult(res, 2))
    {
        std::vector<LedgerObject> objects;
        for (size_t i = 0; i < numRows; ++i)
        {
            objects.push_back({res.asUInt256(i, 0), res.asUnHexedBlob(i, 1)});
        }
        return objects;
    }

    return {};
}

AccountTransactions
PostgresBackend::fetchAccountTransactions(
    ripple::AccountID const& account,
    std::uint32_t const limit,
    bool forward,
    std::optional<AccountTransactionsCursor> const& cursor,
    boost::asio::yield_context& yield) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery(set_timeout, yield);
    pg_params dbParams;

    char const*& command = dbParams.first;
    std::vector<std::optional<std::string>>& values = dbParams.second;
    command =
        "SELECT account_tx($1::bytea, $2::bigint, $3::bool, "
        "$4::bigint, $5::bigint)";
    values.resize(5);
    values[0] = "\\x" + strHex(account);

    values[1] = std::to_string(limit);

    values[2] = std::to_string(forward);

    if (cursor)
    {
        values[3] = std::to_string(cursor->ledgerSequence);
        values[4] = std::to_string(cursor->transactionIndex);
    }
    for (size_t i = 0; i < values.size(); ++i)
    {
        BOOST_LOG_TRIVIAL(debug) << "value " << std::to_string(i) << " = "
                                 << (values[i] ? values[i].value() : "null");
    }

    auto start = std::chrono::system_clock::now();
    auto res = pgQuery(dbParams, yield);
    auto end = std::chrono::system_clock::now();

    auto duration = ((end - start).count()) / 1000000000.0;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " : executed stored_procedure in "
        << std::to_string(duration)
        << " num records = " << std::to_string(checkResult(res, 1));

    checkResult(res, 1);

    char const* resultStr = res.c_str();
    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "postgres result = " << resultStr
                             << " : account = " << strHex(account);

    boost::json::value raw = boost::json::parse(resultStr);
    boost::json::object responseObj = raw.as_object();
    BOOST_LOG_TRIVIAL(debug) << " parsed = " << responseObj;
    if (responseObj.contains("transactions"))
    {
        auto txns = responseObj.at("transactions").as_array();
        std::vector<ripple::uint256> hashes;
        for (auto& hashHex : txns)
        {
            ripple::uint256 hash;
            if (hash.parseHex(hashHex.at("hash").as_string().c_str() + 2))
                hashes.push_back(hash);
        }
        if (responseObj.contains("cursor"))
        {
            return {
                fetchTransactions(hashes, yield),
                {{responseObj.at("cursor").at("ledger_sequence").as_int64(),
                  responseObj.at("cursor")
                      .at("transaction_index")
                      .as_int64()}}};
        }
        return {fetchTransactions(hashes, yield), {}};
    }
    return {{}, {}};
}  // namespace Backend

void
PostgresBackend::open(bool readOnly)
{
    initSchema(pgPool_);
    initAccountTx(pgPool_);
}

void
PostgresBackend::close()
{
}

void
PostgresBackend::startWrites() const
{
    synchronous([&](boost::asio::yield_context yield) {
        numRowsInObjectsBuffer_ = 0;
        abortWrite_ = false;
        auto res = writeConnection_("BEGIN", yield);
        if (!res || res.status() != PGRES_COMMAND_OK)
        {
            std::stringstream msg;
            msg << "Postgres error creating transaction: " << res.msg();
            throw std::runtime_error(msg.str());
        }
    });
}

bool
PostgresBackend::doFinishWrites() const
{
    synchronous([&](boost::asio::yield_context yield) {
        if (!abortWrite_)
        {
            std::string txStr = transactionsBuffer_.str();
            writeConnection_.bulkInsert("transactions", txStr, yield);
            writeConnection_.bulkInsert(
                "account_transactions", accountTxBuffer_.str(), yield);
            std::string objectsStr = objectsBuffer_.str();
            if (objectsStr.size())
                writeConnection_.bulkInsert("objects", objectsStr, yield);
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " objects size = " << objectsStr.size()
                << " txns size = " << txStr.size();
            std::string successorStr = successorBuffer_.str();
            if (successorStr.size())
                writeConnection_.bulkInsert("successor", successorStr, yield);
            if (!range)
            {
                std::stringstream indexCreate;
                indexCreate
                    << "CREATE INDEX diff ON objects USING hash(ledger_seq) "
                       "WHERE NOT "
                       "ledger_seq = "
                    << std::to_string(inProcessLedger);
                writeConnection_(indexCreate.str().data(), yield);
            }
        }
        auto res = writeConnection_("COMMIT", yield);
        if (!res || res.status() != PGRES_COMMAND_OK)
        {
            std::stringstream msg;
            msg << "Postgres error committing transaction: " << res.msg();
            throw std::runtime_error(msg.str());
        }
        transactionsBuffer_.str("");
        transactionsBuffer_.clear();
        objectsBuffer_.str("");
        objectsBuffer_.clear();
        successorBuffer_.str("");
        successorBuffer_.clear();
        successors_.clear();
        accountTxBuffer_.str("");
        accountTxBuffer_.clear();
        numRowsInObjectsBuffer_ = 0;
    });

    return !abortWrite_;
}

bool
PostgresBackend::doOnlineDelete(
    std::uint32_t const numLedgersToKeep,
    boost::asio::yield_context& yield) const
{
    auto rng = fetchLedgerRange();
    if (!rng)
        return false;
    std::uint32_t minLedger = rng->maxSequence - numLedgersToKeep;
    if (minLedger <= rng->minSequence)
        return false;
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 0", yield);
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        auto [objects, curCursor, warning] = retryOnTimeout([&]() {
            return fetchLedgerPage(cursor, minLedger, 256, 0, yield);
        });
        if (warning)
        {
            BOOST_LOG_TRIVIAL(warning) << __func__
                                       << " online delete running but "
                                          "flag ledger is not complete";
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }
        BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched a page";
        std::stringstream objectsBuffer;

        for (auto& obj : objects)
        {
            objectsBuffer << "\\\\x" << ripple::strHex(obj.key) << '\t'
                          << std::to_string(minLedger) << '\t' << "\\\\x"
                          << ripple::strHex(obj.blob) << '\n';
        }
        pgQuery.bulkInsert("objects", objectsBuffer.str(), yield);
        cursor = curCursor;
        if (!cursor)
            break;
    }
    BOOST_LOG_TRIVIAL(info) << __func__ << " finished inserting into objects";
    {
        std::stringstream sql;
        sql << "DELETE FROM ledgers WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data(), yield);
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from ledgers table");
    }
    {
        std::stringstream sql;
        sql << "DELETE FROM keys WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data(), yield);
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from keys table");
    }
    {
        std::stringstream sql;
        sql << "DELETE FROM books WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data(), yield);
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from books table");
    }
    return true;
}

}  // namespace Backend
