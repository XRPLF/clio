#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <backend/PostgresBackend.h>
namespace Backend {

PostgresBackend::PostgresBackend(boost::json::object const& config)
    : BackendInterface(config)
    , pgPool_(make_PgPool(config))
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
    std::string&& ledgerHeader,
    bool isFirst) const
{
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

    auto res = writeConnection_(ledgerInsert.data());
    abortWrite_ = !res;
}

void
PostgresBackend::writeAccountTransactions(
    std::vector<AccountTransactionsData>&& data) const
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
    uint32_t seq,
    std::string&& blob) const
{
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
        writeConnection_.bulkInsert("objects", objectsBuffer_.str());
        BOOST_LOG_TRIVIAL(info) << __func__ << " Flushed large buffer";
        objectsBuffer_.str("");
    }
}

void
PostgresBackend::writeTransaction(
    std::string&& hash,
    uint32_t seq,
    std::string&& transaction,
    std::string&& metadata) const
{
    if (abortWrite_)
        return;
    transactionsBuffer_ << "\\\\x" << ripple::strHex(hash) << '\t'
                        << std::to_string(seq) << '\t' << "\\\\x"
                        << ripple::strHex(transaction) << '\t' << "\\\\x"
                        << ripple::strHex(metadata) << '\n';
}

uint32_t
checkResult(PgResult const& res, uint32_t numFieldsExpected)
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
std::optional<uint32_t>
PostgresBackend::fetchLatestLedgerSequence() const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    auto res = pgQuery(
        "SELECT ledger_seq FROM ledgers ORDER BY ledger_seq DESC LIMIT 1");
    if (checkResult(res, 1))
        return res.asBigInt(0, 0);
    return {};
}

std::optional<ripple::LedgerInfo>
PostgresBackend::fetchLedgerBySequence(uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT * FROM ledgers WHERE ledger_seq = "
        << std::to_string(sequence);
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 10))
        return parseLedgerInfo(res);
    return {};
}

std::optional<ripple::LedgerInfo>
PostgresBackend::fetchLedgerByHash(ripple::uint256 const& hash) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT * FROM ledgers WHERE ledger_hash = "
        << ripple::to_string(hash);
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 10))
        return parseLedgerInfo(res);
    return {};
}

std::optional<LedgerRange>
PostgresBackend::hardFetchLedgerRange() const
{
    auto range = PgQuery(pgPool_)("SELECT complete_ledgers()");
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
PostgresBackend::fetchLedgerObject(
    ripple::uint256 const& key,
    uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT object FROM objects WHERE key = "
        << "\'\\x" << ripple::strHex(key) << "\'"
        << " AND ledger_seq <= " << std::to_string(sequence)
        << " ORDER BY ledger_seq DESC LIMIT 1";
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 1))
    {
        auto blob = res.asUnHexedBlob(0, 0);
        if (blob.size())
            return blob;
    }

    return {};
}

// returns a transaction, metadata pair
std::optional<TransactionAndMetadata>
PostgresBackend::fetchTransaction(ripple::uint256 const& hash) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT transaction,metadata,ledger_seq FROM transactions "
           "WHERE hash = "
        << "\'\\x" << ripple::strHex(hash) << "\'";
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 3))
    {
        return {
            {res.asUnHexedBlob(0, 0),
             res.asUnHexedBlob(0, 1),
             res.asBigInt(0, 2)}};
    }

    return {};
}
std::vector<TransactionAndMetadata>
PostgresBackend::fetchAllTransactionsInLedger(uint32_t ledgerSequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT transaction, metadata, ledger_seq FROM transactions WHERE "
        << "ledger_seq = " << std::to_string(ledgerSequence);
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 3))
    {
        std::vector<TransactionAndMetadata> txns;
        for (size_t i = 0; i < numRows; ++i)
        {
            txns.push_back(
                {res.asUnHexedBlob(i, 0),
                 res.asUnHexedBlob(i, 1),
                 res.asBigInt(i, 2)});
        }
        return txns;
    }
    return {};
}
std::vector<ripple::uint256>
PostgresBackend::fetchAllTransactionHashesInLedger(
    uint32_t ledgerSequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT hash FROM transactions WHERE "
        << "ledger_seq = " << std::to_string(ledgerSequence);
    auto res = pgQuery(sql.str().data());
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

LedgerPage
PostgresBackend::doFetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    auto index = getKeyIndexOfSeq(ledgerSequence);
    if (!index)
        return {};
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT key FROM keys WHERE ledger_seq = "
        << std::to_string(index->keyIndex);
    if (cursor)
        sql << " AND key >= \'\\x" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY key ASC LIMIT " << std::to_string(limit);
    BOOST_LOG_TRIVIAL(debug) << __func__ << sql.str();
    auto res = pgQuery(sql.str().data());
    BOOST_LOG_TRIVIAL(debug) << __func__ << " fetched keys";
    std::optional<ripple::uint256> returnCursor;
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<ripple::uint256> keys;
        for (size_t i = 0; i < numRows; ++i)
        {
            keys.push_back({res.asUInt256(i, 0)});
        }
        if (numRows >= limit)
        {
            returnCursor = keys.back();
            ++(*returnCursor);
        }

        auto objs = fetchLedgerObjects(keys, ledgerSequence);
        std::vector<LedgerObject> results;
        for (size_t i = 0; i < objs.size(); ++i)
        {
            if (objs[i].size())
            {
                results.push_back({keys[i], objs[i]});
            }
        }
        if (!cursor && !keys[0].isZero())
            return {results, returnCursor, "Data may be incomplete"};
        return {results, returnCursor};
    }
    if (!cursor)
        return {{}, {}, "Data may be incomplete"};
    return {};
}

std::vector<TransactionAndMetadata>
PostgresBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes) const
{
    std::vector<TransactionAndMetadata> results;
    constexpr bool doAsync = true;
    if (doAsync)
    {
        auto start = std::chrono::system_clock::now();
        auto end = std::chrono::system_clock::now();
        auto duration = ((end - start).count()) / 1000000000.0;
        results.resize(hashes.size());
        std::condition_variable cv;
        std::mutex mtx;
        std::atomic_uint numRemaining = hashes.size();
        for (size_t i = 0; i < hashes.size(); ++i)
        {
            auto const& hash = hashes[i];
            boost::asio::post(
                pool_, [this, &hash, &results, &numRemaining, &cv, &mtx, i]() {
                    BOOST_LOG_TRIVIAL(debug)
                        << __func__ << " getting txn = " << i;
                    PgQuery pgQuery(pgPool_);
                    std::stringstream sql;
                    sql << "SELECT transaction,metadata,ledger_seq FROM "
                           "transactions "
                           "WHERE HASH = \'\\x"
                        << ripple::strHex(hash) << "\'";

                    auto res = pgQuery(sql.str().data());
                    if (size_t numRows = checkResult(res, 3))
                    {
                        results[i] = {
                            res.asUnHexedBlob(0, 0),
                            res.asUnHexedBlob(0, 1),
                            res.asBigInt(0, 2)};
                    }
                    if (--numRemaining == 0)
                    {
                        std::unique_lock lck(mtx);
                        cv.notify_one();
                    }
                });
        }
        std::unique_lock lck(mtx);
        cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
        auto end2 = std::chrono::system_clock::now();
        duration = ((end2 - end).count()) / 1000000000.0;
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " fetched " << std::to_string(hashes.size())
            << " transactions with threadpool. took "
            << std::to_string(duration);
    }
    else
    {
        PgQuery pgQuery(pgPool_);
        pgQuery("SET statement_timeout TO 10000");
        std::stringstream sql;
        for (size_t i = 0; i < hashes.size(); ++i)
        {
            auto const& hash = hashes[i];
            sql << "SELECT transaction,metadata,ledger_seq FROM "
                   "transactions "
                   "WHERE HASH = \'\\x"
                << ripple::strHex(hash) << "\'";
            if (i + 1 < hashes.size())
                sql << " UNION ALL ";
        }
        auto start = std::chrono::system_clock::now();
        auto res = pgQuery(sql.str().data());
        auto end = std::chrono::system_clock::now();
        auto duration = ((end - start).count()) / 1000000000.0;
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " fetched " << std::to_string(hashes.size())
            << " transactions with union all. took "
            << std::to_string(duration);
        if (size_t numRows = checkResult(res, 3))
        {
            for (size_t i = 0; i < numRows; ++i)
                results.push_back(
                    {res.asUnHexedBlob(i, 0),
                     res.asUnHexedBlob(i, 1),
                     res.asBigInt(i, 2)});
        }
    }
    return results;
}

std::vector<Blob>
PostgresBackend::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::vector<Blob> results;
    results.resize(keys.size());
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic_uint numRemaining = keys.size();
    auto start = std::chrono::system_clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
    {
        auto const& key = keys[i];
        boost::asio::post(
            pool_,
            [this, &key, &results, &numRemaining, &cv, &mtx, i, sequence]() {
                PgQuery pgQuery(pgPool_);
                std::stringstream sql;
                sql << "SELECT object FROM "
                       "objects "
                       "WHERE key = \'\\x"
                    << ripple::strHex(key) << "\'"
                    << " AND ledger_seq <= " << std::to_string(sequence)
                    << " ORDER BY ledger_seq DESC LIMIT 1";

                auto res = pgQuery(sql.str().data());
                if (size_t numRows = checkResult(res, 1))
                {
                    results[i] = res.asUnHexedBlob();
                }
                if (--numRemaining == 0)
                {
                    std::unique_lock lck(mtx);
                    cv.notify_one();
                }
            });
    }
    std::unique_lock lck(mtx);
    cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
    auto end = std::chrono::system_clock::now();
    auto duration = ((end - start).count()) / 1000000000.0;
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " fetched " << std::to_string(keys.size())
        << " objects with threadpool. took " << std::to_string(duration);
    return results;
}

AccountTransactions
PostgresBackend::fetchAccountTransactions(
    ripple::AccountID const& account,
    std::uint32_t limit,
    bool forward,
    std::optional<AccountTransactionsCursor> const& cursor) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    pg_params dbParams;

    char const*& command = dbParams.first;
    std::vector<std::optional<std::string>>& values = dbParams.second;
    command =
        "SELECT account_tx($1::bytea, $2::bigint, $3::bool"
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
    auto res = pgQuery(dbParams);
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
                fetchTransactions(hashes),
                {{responseObj.at("cursor").at("ledger_sequence").as_int64(),
                  responseObj.at("cursor")
                      .at("transaction_index")
                      .as_int64()}}};
        }
        return {fetchTransactions(hashes), {}};
    }
    return {{}, {}};
}  // namespace Backend

void
PostgresBackend::open(bool readOnly)
{
    if (!readOnly)
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
    numRowsInObjectsBuffer_ = 0;
    abortWrite_ = false;
    auto res = writeConnection_("BEGIN");
    if (!res || res.status() != PGRES_COMMAND_OK)
    {
        std::stringstream msg;
        msg << "Postgres error creating transaction: " << res.msg();
        throw std::runtime_error(msg.str());
    }
}

bool
PostgresBackend::doFinishWrites() const
{
    if (!abortWrite_)
    {
        std::string txStr = transactionsBuffer_.str();
        writeConnection_.bulkInsert("transactions", txStr);
        writeConnection_.bulkInsert(
            "account_transactions", accountTxBuffer_.str());
        std::string objectsStr = objectsBuffer_.str();
        if (objectsStr.size())
            writeConnection_.bulkInsert("objects", objectsStr);
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " objects size = " << objectsStr.size()
            << " txns size = " << txStr.size();
    }
    auto res = writeConnection_("COMMIT");
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
    accountTxBuffer_.str("");
    accountTxBuffer_.clear();
    numRowsInObjectsBuffer_ = 0;
    return !abortWrite_;
}
bool
PostgresBackend::writeKeys(
    std::unordered_set<ripple::uint256> const& keys,
    KeyIndex const& index,
    bool isAsync) const
{
    if (abortWrite_)
        return false;
    PgQuery pgQuery(pgPool_);
    PgQuery& conn = isAsync ? pgQuery : writeConnection_;
    std::stringstream sql;
    size_t numRows = 0;
    for (auto& key : keys)
    {
        numRows++;
        sql << "INSERT INTO keys (ledger_seq, key) VALUES ("
            << std::to_string(index.keyIndex) << ", \'\\x"
            << ripple::strHex(key) << "\') ON CONFLICT DO NOTHING; ";
        if (numRows > 10000)
        {
            conn(sql.str().c_str());
            sql.str("");
            sql.clear();
            numRows = 0;
        }
    }
    if (numRows > 0)
        conn(sql.str().c_str());
    return true;
    /*
    BOOST_LOG_TRIVIAL(debug) << __func__;
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic_uint numRemaining = keys.size();
    auto start = std::chrono::system_clock::now();
    for (auto& key : keys)
    {
        boost::asio::post(
            pool_, [this, key, &numRemaining, &cv, &mtx, &index]() {
                PgQuery pgQuery(pgPool_);
                std::stringstream sql;
                sql << "INSERT INTO keys (ledger_seq, key) VALUES ("
                    << std::to_string(index.keyIndex) << ", \'\\x"
                    << ripple::strHex(key) << "\') ON CONFLICT DO NOTHING";

                auto res = pgQuery(sql.str().data());
                if (--numRemaining == 0)
                {
                    std::unique_lock lck(mtx);
                    cv.notify_one();
                }
            });
    }
    std::unique_lock lck(mtx);
    cv.wait(lck, [&numRemaining]() { return numRemaining == 0; });
    auto end = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " wrote " << std::to_string(keys.size())
        << " keys with threadpool. took " << std::to_string(duration);
        */
    return true;
}
bool
PostgresBackend::doOnlineDelete(uint32_t numLedgersToKeep) const
{
    auto rng = fetchLedgerRange();
    if (!rng)
        return false;
    uint32_t minLedger = rng->maxSequence - numLedgersToKeep;
    if (minLedger <= rng->minSequence)
        return false;
    uint32_t limit = 2048;
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 0");
    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                fetchLedgerPage(cursor, minLedger, 256);
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
            pgQuery.bulkInsert("objects", objectsBuffer.str());
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
    BOOST_LOG_TRIVIAL(info) << __func__ << " finished inserting into objects";
    {
        std::stringstream sql;
        sql << "DELETE FROM ledgers WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data());
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from ledgers table");
    }
    {
        std::stringstream sql;
        sql << "DELETE FROM keys WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data());
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from keys table");
    }
    {
        std::stringstream sql;
        sql << "DELETE FROM books WHERE ledger_seq < "
            << std::to_string(minLedger);
        auto res = pgQuery(sql.str().data());
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from books table");
    }
    return true;
}

}  // namespace Backend
