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
    PgQuery pgQuery(pgPool_);
    BOOST_LOG_TRIVIAL(debug) << __func__;
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
    BOOST_LOG_TRIVIAL(trace) << __func__ << " : "
                             << " : "
                             << "query string = " << ledgerInsert;

    auto res = pgQuery(ledgerInsert.data());

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
        std::string txHash = ripple::strHex(record.txHash);
        auto idx = record.transactionIndex;
        auto ledgerSeq = record.ledgerSequence;

        for (auto const& a : record.accounts)
        {
            std::string acct = ripple::strHex(a);
            accountTxBuffer_ << "\\\\x" << acct << '\t'
                             << std::to_string(ledgerSeq) << '\t'
                             << std::to_string(idx) << '\t' << "\\\\x"
                             << ripple::strHex(txHash);
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
    if (abortWrite_)
        return;
    objectsBuffer_ << "\\\\x" << ripple::strHex(key) << '\t'
                   << std::to_string(seq) << '\t' << "\\\\x"
                   << ripple::strHex(blob) << '\n';

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
        assert(false);
        throw std::runtime_error("null postgres response");
    }
    else if (res.status() != PGRES_TUPLES_OK)
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
    char const* hash = res.c_str(0, 0);
    char const* prevHash = res.c_str(0, 1);
    char const* accountHash = res.c_str(0, 2);
    char const* txHash = res.c_str(0, 3);
    std::int64_t totalCoins = res.asBigInt(0, 4);
    std::int64_t closeTime = res.asBigInt(0, 5);
    std::int64_t parentCloseTime = res.asBigInt(0, 6);
    std::int64_t closeTimeRes = res.asBigInt(0, 7);
    std::int64_t closeFlags = res.asBigInt(0, 8);
    std::int64_t ledgerSeq = res.asBigInt(0, 9);

    using time_point = ripple::NetClock::time_point;
    using duration = ripple::NetClock::duration;

    ripple::LedgerInfo info;
    if (!info.parentHash.parseHex(prevHash + 2))
        throw std::runtime_error("parseLedgerInfo - error parsing parent hash");
    if (!info.txHash.parseHex(txHash + 2))
        throw std::runtime_error("parseLedgerInfo - error parsing tx map hash");
    if (!info.accountHash.parseHex(accountHash + 2))
        throw std::runtime_error(
            "parseLedgerInfo - error parsing state map hash");
    info.drops = totalCoins;
    info.closeTime = time_point{duration{closeTime}};
    info.parentCloseTime = time_point{duration{parentCloseTime}};
    info.closeFlags = closeFlags;
    info.closeTimeResolution = duration{closeTimeRes};
    info.seq = ledgerSeq;
    if (!info.hash.parseHex(hash + 2))
        throw std::runtime_error("parseLedgerInfo - error parsing ledger hash");
    info.validated = true;
    return info;
}
std::optional<uint32_t>
PostgresBackend::fetchLatestLedgerSequence() const
{
    PgQuery pgQuery(pgPool_);
    auto res =
        pgQuery("SELECT sequence FROM ledgers ORDER BY sequence DESC LIMIT 1");
    if (checkResult(res, 1))
        return res.asBigInt(0, 0);
    return {};
}

std::optional<ripple::LedgerInfo>
PostgresBackend::fetchLedgerBySequence(uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT * FROM ledgers WHERE sequence = "
        << std::to_string(sequence);
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 10))
        return parseLedgerInfo(res);
    return {};
}

std::optional<Blob>
PostgresBackend::fetchLedgerObject(
    ripple::uint256 const& key,
    uint32_t sequence) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT object FROM objects WHERE key = "
        << "\'\\x" << ripple::strHex(key) << "\'"
        << " AND sequence <= " << std::to_string(sequence)
        << " ORDER BY sequence DESC LIMIT 1";
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 1))
    {
        char const* object = res.c_str(0, 0);
        std::string_view view{object};
        std::vector<unsigned char> blob{view.front(), view.back()};
        return blob;
    }

    return {};
}

// returns a transaction, metadata pair
std::optional<TransactionAndMetadata>
PostgresBackend::fetchTransaction(ripple::uint256 const& hash) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT transaction,metadata,ledger_sequence FROM transactions "
           "WHERE hash = "
        << "\'\\x" << ripple::strHex(hash) << "\'";
    auto res = pgQuery(sql.str().data());
    if (checkResult(res, 3))
    {
        char const* txn = res.c_str(0, 0);
        char const* metadata = res.c_str(0, 1);
        std::string_view txnView{txn};
        std::string_view metadataView{metadata};
        return {
            {{txnView.front(), txnView.back()},
             {metadataView.front(), metadataView.back()}}};
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
    std::stringstream sql;
    sql << "SELECT key,object FROM"
        << " (SELECT DISTINCT ON (key) * FROM objects"
        << " WHERE sequence <= " << std::to_string(ledgerSequence);
    if (cursor)
        sql << " AND key > \'x\\" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY key, sequence DESC) sub"
        << " WHERE object != \'\\x\'"
        << " LIMIT " << std::to_string(limit);
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 2))
    {
        std::vector<LedgerObject> objects;
        for (size_t i = 0; i < numRows; ++i)
        {
            ripple::uint256 key;
            if (!key.parseHex(res.c_str(i, 0)))
                throw std::runtime_error("Error parsing key from postgres");
            char const* object = res.c_str(i, 1);
            std::string_view view{object};
            objects.push_back({std::move(key), {view.front(), view.back()}});
        }
        if (numRows == limit)
            return {objects, objects[objects.size() - 1].key};
        else
            return {objects, {}};
    }
    return {};
}

std::vector<LedgerObject>
PostgresBackend::fetchBookOffers(
    ripple::uint256 const& book,
    uint32_t ledgerSequence,
    std::optional<ripple::uint256> const& cursor) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT key FROM"
        << " (SELECT DISTINCT ON (key) * FROM books WHERE book = "
        << "\'\\x" << ripple::strHex(book)
        << "\' AND sequence <= " << std::to_string(ledgerSequence);
    if (cursor)
        sql << " AND key > \'" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY key DESC, sequence DESC)"
        << " sub WHERE NOT deleted";
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<ripple::uint256> keys;
        for (size_t i = 0; i < numRows; ++i)
        {
            ripple::uint256 key;
            if (!key.parseHex(res.c_str(i, 0)))
                throw std::runtime_error("Error parsing key from postgres");
            keys.push_back(std::move(key));
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
        return results;
    }
    return {};
}

std::vector<TransactionAndMetadata>
PostgresBackend::fetchTransactions(
    std::vector<ripple::uint256> const& hashes) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT transaction,metadata,ledger_sequence FROM transactions "
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
            char const* txn = res.c_str(i, 0);
            char const* metadata = res.c_str(i, 1);
            std::string_view txnView{txn};
            std::string_view metadataView{metadata};

            results.push_back(
                {{txnView.front(), txnView.back()},
                 {metadataView.front(), metadataView.back()}});
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
    std::stringstream sql;
    sql << "SELECT object FROM objects WHERE";

    bool first = true;
    for (auto const& key : keys)
    {
        if (!first)
        {
            sql << " OR ";
            first = false;
        }
        else
        {
            sql << " ( ";
        }
        sql << " key = "
            << "\'\\x" << ripple::strHex(key) << "\'";
    }
    sql << " ) "
        << " AND sequence <= " << std::to_string(sequence)
        << " ORDER BY sequence DESC LIMIT 1";
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 1))
    {
        std::vector<Blob> results;
        for (size_t i = 0; i < numRows; ++i)
        {
            char const* object = res.c_str(i, 0);
            std::string_view view{object};
            results.push_back({view.front(), view.back()});
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
    std::optional<AccountTransactionsCursor> const& cursor) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream sql;
    sql << "SELECT hash, ledger_sequence, transaction_index FROM "
           "account_transactions WHERE account = "
        << ripple::strHex(account);
    if (cursor)
        sql << " AND ledger_sequence < " << cursor->ledgerSequence
            << " AND transaction_index < " << cursor->transactionIndex;
    uint32_t limit = 300;
    sql << " LIMIT " << std::to_string(limit);
    auto res = pgQuery(sql.str().data());
    if (size_t numRows = checkResult(res, 3))
    {
        std::vector<ripple::uint256> hashes;
        for (size_t i = 0; i < numRows; ++i)
        {
            ripple::uint256 hash;
            if (!hash.parseHex(res.c_str(i, 0)))
                throw std::runtime_error(
                    "Error parsing transaction hash from Postgres");
            hashes.push_back(std::move(hash));
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
}

void
PostgresBackend::close()
{
}

void
PostgresBackend::startWrites() const
{
    PgQuery pg(pgPool_);
    auto res = pg("BEGIN");
    if (!res || res.status() != PGRES_COMMAND_OK)
    {
        std::stringstream msg;
        msg << "Postgres error creating transaction: " << res.msg();
        throw std::runtime_error(msg.str());
    }
}

bool
PostgresBackend::finishWrites() const
{
    if (abortWrite_)
        return false;
    PgQuery pg(pgPool_);
    pg.bulkInsert("transactions", transactionsBuffer_.str());
    pg.bulkInsert("objects", objectsBuffer_.str());
    pg.bulkInsert("books", booksBuffer_.str());
    pg.bulkInsert("account_transactions", accountTxBuffer_.str());
    auto res = pg("COMMIT");
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
    return true;
}

}  // namespace Backend
