#include <boost/format.hpp>
#include <reporting/PostgresBackend.h>
namespace Backend {

PostgresBackend::PostgresBackend(boost::json::object const& config)
    : pgPool_(make_PgPool(config))
{
}
void
PostgresBackend::writeLedger(
    ripple::LedgerInfo const& ledgerInfo,
    std::string&& ledgerHeader,
    bool isFirst) const
{
    ledgerHeader_ = ledgerInfo;
}

void
PostgresBackend::writeAccountTransactions(
    std::vector<AccountTransactionsData>&& data) const
{
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
PostgresBackend::writeLedgerObject(
    std::string&& key,
    uint32_t seq,
    std::string&& blob,
    bool isCreated,
    bool isDeleted,
    std::optional<ripple::uint256>&& book) const
{
    objectsBuffer_ << "\\\\x" << ripple::strHex(key) << '\t'
                   << std::to_string(seq) << '\t' << "\\\\x"
                   << ripple::strHex(blob) << '\n';
    numRowsInObjectsBuffer_++;
    // If the buffer gets too large, the insert fails. Not sure why. So we
    // insert after 1 million records
    if (numRowsInObjectsBuffer_ % 1000000 == 0)
    {
        PgQuery pgQuery(pgPool_);
        pgQuery.bulkInsert("objects", objectsBuffer_.str());
        objectsBuffer_ = {};
    }

    if (book)
    {
        booksBuffer_ << "\\\\x" << ripple::strHex(*book) << '\t'
                     << std::to_string(seq) << '\t' << isDeleted << '\t'
                     << "\\\\x" << ripple::strHex(key) << '\n';
    }
}

void
PostgresBackend::writeTransaction(
    std::string&& hash,
    uint32_t seq,
    std::string&& transaction,
    std::string&& metadata) const
{
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
        return res.asUnHexedBlob(0, 0);
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
    if (size_t numRows = checkResult(res, 3))
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
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT key,object FROM"
        << " (SELECT DISTINCT ON (key) * FROM objects"
        << " WHERE ledger_seq <= " << std::to_string(ledgerSequence);
    if (cursor)
        sql << " AND key < \'\\x" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY key DESC, ledger_seq DESC) sub"
        << " WHERE object != \'\\x\'"
        << " LIMIT " << std::to_string(limit);
    BOOST_LOG_TRIVIAL(debug) << __func__ << sql.str();
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 2))
    {
        std::vector<LedgerObject> objects;
        for (size_t i = 0; i < numRows; ++i)
        {
            objects.push_back({res.asUInt256(i, 0), res.asUnHexedBlob(i, 1)});
        }
        if (numRows == limit)
            return {objects, objects[objects.size() - 1].key};
        else
            return {objects, {}};
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
    sql << "SELECT offer_key FROM"
        << " (SELECT DISTINCT ON (offer_key) * FROM books WHERE book = "
        << "\'\\x" << ripple::strHex(book)
        << "\' AND ledger_seq <= " << std::to_string(ledgerSequence);
    if (cursor)
        sql << " AND offer_key > \'" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY offer_key DESC, ledger_seq DESC)"
        << " sub WHERE NOT deleted"
        << " LIMIT " << std::to_string(limit);
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
        return {results, results[results.size() - 1].key};
    }
    return {{}, {}};
}

std::vector<TransactionAndMetadata>
PostgresBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT transaction,metadata,ledger_seq FROM transactions "
           "WHERE ";
    bool first = true;
    for (auto const& hash : hashes)
    {
        if (!first)
            sql << " OR ";
        sql << "HASH = \'\\x" << ripple::strHex(hash) << "\'";
        first = false;
    }
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 3))
    {
        std::vector<TransactionAndMetadata> results;
        for (size_t i = 0; i < numRows; ++i)
        {
            results.push_back(
                {res.asUnHexedBlob(i, 0),
                 res.asUnHexedBlob(i, 1),
                 res.asBigInt(i, 2)});
        }
        return results;
    }

    return {};
}

std::vector<Blob>
PostgresBackend::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    pgQuery("SET statement_timeout TO 10000");
    std::stringstream sql;
    sql << "SELECT DISTINCT ON(key) object FROM objects WHERE";

    bool first = true;
    for (auto const& key : keys)
    {
        if (!first)
        {
            sql << " OR ";
        }
        else
        {
            sql << " ( ";
            first = false;
        }
        sql << " key = "
            << "\'\\x" << ripple::strHex(key) << "\'";
    }
    sql << " ) "
        << " AND ledger_seq <= " << std::to_string(sequence)
        << " ORDER BY key, ledger_seq DESC";

    BOOST_LOG_TRIVIAL(info) << sql.str();
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<Blob> results;
        for (size_t i = 0; i < numRows; ++i)
        {
            results.push_back(res.asUnHexedBlob(i, 0));
        }
        return results;
    }
    return {};
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
    std::stringstream sql;
    sql << "SELECT hash, ledger_seq, transaction_index FROM "
           "account_transactions WHERE account = "
        << "\'\\x" << ripple::strHex(account) << "\'";
    if (cursor)
        sql << " AND (ledger_seq < " << cursor->ledgerSequence
            << " OR (ledger_seq = " << cursor->ledgerSequence
            << " AND transaction_index < " << cursor->transactionIndex << "))";
    sql << " ORDER BY ledger_seq DESC, transaction_index DESC";
    sql << " LIMIT " << std::to_string(limit);
    BOOST_LOG_TRIVIAL(debug) << __func__ << " : " << sql.str();
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 3))
    {
        std::vector<ripple::uint256> hashes;
        for (size_t i = 0; i < numRows; ++i)
        {
            hashes.push_back(res.asUInt256(i, 0));
        }

        if (numRows == limit)
        {
            AccountTransactionsCursor retCursor{
                res.asBigInt(numRows - 1, 1), res.asBigInt(numRows - 1, 2)};
            return {fetchTransactions(hashes), {retCursor}};
        }
        else
        {
            return {fetchTransactions(hashes), {}};
        }
    }
    return {};
}

void
PostgresBackend::open()
{
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
}

bool
PostgresBackend::finishWrites() const
{
    PgQuery pg(pgPool_);
    auto res = pg("BEGIN");
    if (!res || res.status() != PGRES_COMMAND_OK)
    {
        std::stringstream msg;
        msg << "Postgres error creating transaction: " << res.msg();
        throw std::runtime_error(msg.str());
    }
    auto cmd = boost::format(
        R"(INSERT INTO ledgers
           VALUES (%u,'\x%s', '\x%s',%u,%u,%u,%u,%u,'\x%s','\x%s'))");

    auto ledgerInsert = boost::str(
        cmd % ledgerHeader_.seq % ripple::strHex(ledgerHeader_.hash) %
        ripple::strHex(ledgerHeader_.parentHash) % ledgerHeader_.drops.drops() %
        ledgerHeader_.closeTime.time_since_epoch().count() %
        ledgerHeader_.parentCloseTime.time_since_epoch().count() %
        ledgerHeader_.closeTimeResolution.count() % ledgerHeader_.closeFlags %
        ripple::strHex(ledgerHeader_.accountHash) %
        ripple::strHex(ledgerHeader_.txHash));

    res = pg(ledgerInsert.data());
    if (res)
    {
        pg.bulkInsert("transactions", transactionsBuffer_.str());
        pg.bulkInsert("books", booksBuffer_.str());
        pg.bulkInsert("account_transactions", accountTxBuffer_.str());
        std::string objectsStr = objectsBuffer_.str();
        if (objectsStr.size())
            pg.bulkInsert("objects", objectsStr);
    }
    res = pg("COMMIT");
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
    return true;
}

}  // namespace Backend
