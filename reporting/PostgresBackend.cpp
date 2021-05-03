#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <reporting/PostgresBackend.h>
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
    std::string&& blob,
    bool isCreated,
    bool isDeleted,
    std::optional<ripple::uint256>&& book) const
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

std::optional<LedgerRange>
PostgresBackend::fetchLedgerRange() const
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
PostgresBackend::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    auto index = getIndexOfSeq(ledgerSequence);
    if (!index)
        return {};
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT key FROM keys WHERE ledger_seq = " << std::to_string(*index);
    if (cursor)
        sql << " AND key < \'\\x" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY key DESC LIMIT " << std::to_string(limit);
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
        if (numRows == limit)
            returnCursor = keys.back();

        auto objs = fetchLedgerObjects(keys, ledgerSequence);
        std::vector<LedgerObject> results;
        for (size_t i = 0; i < objs.size(); ++i)
        {
            if (objs[i].size())
            {
                results.push_back({keys[i], objs[i]});
            }
        }
        return {results, returnCursor};
    }
    return {};
}

std::pair<std::vector<LedgerObject>, std::optional<ripple::uint256>>
PostgresBackend::fetchBookOffers(
    ripple::uint256 const& book,
    uint32_t ledgerSequence,
    std::uint32_t limit,
    std::optional<ripple::uint256> const& cursor) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT offer_key FROM books WHERE book = "
        << "\'\\x" << ripple::strHex(book)
        << "\' AND ledger_seq = " << std::to_string(ledgerSequence);
    if (cursor)
        sql << " AND offer_key < \'\\x" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY offer_key DESC, ledger_seq DESC"
        << " LIMIT " << std::to_string(limit);
    BOOST_LOG_TRIVIAL(debug) << sql.str();
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<ripple::uint256> keys;
        for (size_t i = 0; i < numRows; ++i)
        {
            keys.push_back(res.asUInt256(i, 0));
        }
        std::vector<Blob> blobs = fetchLedgerObjects(keys, ledgerSequence);

        std::vector<LedgerObject> results;
        std::transform(
            blobs.begin(),
            blobs.end(),
            keys.begin(),
            std::back_inserter(results),
            [](auto& blob, auto& key) {
                return LedgerObject{std::move(key), std::move(blob)};
            });
        BOOST_LOG_TRIVIAL(debug) << __func__ << " : " << results.size();
        if (results.size() == limit)
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " : " << ripple::strHex(results[0].key) << " : "
                << ripple::strHex(results[results.size() - 1].key);
            return {results, results[results.size() - 1].key};
        }
        else
            return {results, {}};
    }
    return {{}, {}};
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
                    results[i] = res.asUnHexedBlob(0, 0);
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

std::pair<
    std::vector<TransactionAndMetadata>,
    std::optional<AccountTransactionsCursor>>
PostgresBackend::fetchAccountTransactions(
    ripple::AccountID const& account,
    std::uint32_t limit,
    std::optional<AccountTransactionsCursor> const& cursor) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    pg_params dbParams;

    char const*& command = dbParams.first;
    std::vector<std::optional<std::string>>& values = dbParams.second;
    command =
        "SELECT account_tx($1::bytea, $2::bigint, "
        "$3::bigint, $4::bigint)";
    values.resize(4);
    values[0] = "\\x" + strHex(account);

    values[1] = std::to_string(limit);

    if (cursor)
    {
        values[2] = std::to_string(cursor->ledgerSequence);
        values[3] = std::to_string(cursor->transactionIndex);
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
        writeConnection_.bulkInsert("transactions", transactionsBuffer_.str());
        writeConnection_.bulkInsert(
            "account_transactions", accountTxBuffer_.str());
        std::string objectsStr = objectsBuffer_.str();
        if (objectsStr.size())
            writeConnection_.bulkInsert("objects", objectsStr);
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
    booksBuffer_.str("");
    booksBuffer_.clear();
    accountTxBuffer_.str("");
    accountTxBuffer_.clear();
    numRowsInObjectsBuffer_ = 0;
    return !abortWrite_;
}
bool
PostgresBackend::writeKeys(
    std::unordered_set<ripple::uint256> const& keys,
    uint32_t ledgerSequence) const
{
    BOOST_LOG_TRIVIAL(debug) << __func__;
    PgQuery pgQuery(pgPool_);
    pgQuery("BEGIN");
    std::stringstream keysBuffer;
    size_t numRows = 0;
    for (auto& key : keys)
    {
        keysBuffer << std::to_string(ledgerSequence) << '\t' << "\\\\x"
                   << ripple::strHex(key) << '\n';
        numRows++;
        // If the buffer gets too large, the insert fails. Not sure why. So we
        // insert after 1 million records
        if (numRows == 1000000)
        {
            pgQuery.bulkInsert("keys", keysBuffer.str());
            std::stringstream temp;
            keysBuffer.swap(temp);
            numRows = 0;
        }
    }
    if (numRows > 0)
    {
        pgQuery.bulkInsert("keys", keysBuffer.str());
    }
    pgQuery("COMMIT");
    return true;
}
bool
PostgresBackend::writeBooks(
    std::unordered_map<
        ripple::uint256,
        std::unordered_set<ripple::uint256>> const& books,
    uint32_t ledgerSequence) const
{
    BOOST_LOG_TRIVIAL(debug) << __func__;
    PgQuery pgQuery(pgPool_);
    pgQuery("BEGIN");
    std::stringstream booksBuffer;
    size_t numRows = 0;
    for (auto& book : books)
    {
        for (auto& offer : book.second)
        {
            booksBuffer << std::to_string(ledgerSequence) << '\t' << "\\\\x"
                        << ripple::strHex(book.first) << '\t' << "\\\\x"
                        << ripple::strHex(offer) << '\n';
            numRows++;
            // If the buffer gets too large, the insert fails. Not sure why. So
            // we insert after 1 million records
            if (numRows == 1000000)
            {
                pgQuery.bulkInsert("books", booksBuffer.str());
                std::stringstream temp;
                booksBuffer.swap(temp);
                numRows = 0;
            }
        }
    }
    if (numRows > 0)
    {
        pgQuery.bulkInsert("books", booksBuffer.str());
    }
    pgQuery("COMMIT");
    return true;
}
bool
PostgresBackend::doOnlineDelete(uint32_t minLedgerToKeep) const
{
    uint32_t limit = 2048;
    PgQuery pgQuery(pgPool_);
    {
        std::stringstream sql;
        sql << "DELETE FROM ledgers WHERE ledger_seq < "
            << std::to_string(minLedgerToKeep);
        auto res = pgQuery(sql.str().data());
        if (res.msg() != "ok")
            throw std::runtime_error("Error deleting from ledgers table");
    }

    std::string cursor;
    do
    {
        std::stringstream sql;
        sql << "SELECT DISTINCT ON (key) key,ledger_seq,object FROM objects"
            << " WHERE ledger_seq <= " << std::to_string(minLedgerToKeep);
        if (cursor.size())
            sql << " AND key < \'\\x" << cursor << "\'";
        sql << " ORDER BY key DESC, ledger_seq DESC"
            << " LIMIT " << std::to_string(limit);
        BOOST_LOG_TRIVIAL(trace) << __func__ << sql.str();
        auto res = pgQuery(sql.str().data());
        BOOST_LOG_TRIVIAL(debug) << __func__ << "Fetched a page";
        if (size_t numRows = checkResult(res, 3))
        {
            std::stringstream deleteSql;
            std::stringstream deleteOffersSql;
            deleteSql << "DELETE FROM objects WHERE (";
            deleteOffersSql << "DELETE FROM books WHERE (";
            bool firstOffer = true;
            for (size_t i = 0; i < numRows; ++i)
            {
                std::string_view keyView{res.c_str(i, 0) + 2};
                int64_t sequence = res.asBigInt(i, 1);
                std::string_view objView{res.c_str(i, 2) + 2};
                if (i != 0)
                    deleteSql << " OR ";

                deleteSql << "(key = "
                          << "\'\\x" << keyView << "\'";
                if (objView.size() == 0)
                    deleteSql << " AND ledger_seq <= "
                              << std::to_string(sequence);
                else
                    deleteSql << " AND ledger_seq < "
                              << std::to_string(sequence);
                deleteSql << ")";
                bool deleteOffer = false;
                if (objView.size())
                {
                    deleteOffer = isOfferHex(objView);
                }
                else
                {
                    // This is rather unelegant. For a deleted object, we
                    // don't know its type just from the key (or do we?).
                    // So, we just assume it is an offer and try to delete
                    // it. The alternative is to read the actual object out
                    // of the db from before it was deleted. This could
                    // result in a lot of individual reads though, so we
                    // chose to just delete
                    deleteOffer = true;
                }
                if (deleteOffer)
                {
                    if (!firstOffer)
                        deleteOffersSql << " OR ";
                    deleteOffersSql << "( offer_key = "
                                    << "\'\\x" << keyView << "\')";
                    firstOffer = false;
                }
            }
            if (numRows == limit)
                cursor = res.c_str(numRows - 1, 0) + 2;
            else
                cursor = {};
            deleteSql << ")";
            deleteOffersSql << ")";
            BOOST_LOG_TRIVIAL(trace) << __func__ << deleteSql.str();
            res = pgQuery(deleteSql.str().data());
            if (res.msg() != "ok")
                throw std::runtime_error("Error deleting from objects table");
            if (!firstOffer)
            {
                BOOST_LOG_TRIVIAL(trace) << __func__ << deleteOffersSql.str();
                res = pgQuery(deleteOffersSql.str().data());
                if (res.msg() != "ok")
                    throw std::runtime_error("Error deleting from books table");
            }
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << "Deleted a page. Cursor = " << cursor;
        }
    } while (cursor.size());
    return true;
}

}  // namespace Backend
