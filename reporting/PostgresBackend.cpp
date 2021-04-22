#include <boost/format.hpp>
#include <reporting/PostgresBackend.h>
namespace Backend {

PostgresBackend::PostgresBackend(boost::json::object const& config)
    : BackendInterface(config)
    , pgPool_(make_PgPool(config))
    , writeConnection_(pgPool_)
{
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
    if (numRowsInObjectsBuffer_ % 1000000 == 0)
    {
        writeConnection_.bulkInsert("objects", objectsBuffer_.str());
        objectsBuffer_.str("");
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
        sql << " AND offer_key < \'\\x" << ripple::strHex(*cursor) << "\'";
    sql << " ORDER BY offer_key DESC, ledger_seq DESC)"
        << " sub WHERE NOT deleted"
        << " ORDER BY offer_key DESC "
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
        << " ORDER BY key DESC, ledger_seq DESC";

    BOOST_LOG_TRIVIAL(trace) << sql.str();
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
        writeConnection_.bulkInsert("books", booksBuffer_.str());
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
    PgQuery pgQuery(pgPool_);
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
            keysBuffer.str("");
            numRows = 0;
        }
    }
    if (numRows > 0)
    {
        pgQuery.bulkInsert("keys", keysBuffer.str());
    }
}
bool
PostgresBackend::writeBooks(
    std::unordered_map<
        ripple::uint256,
        std::unordered_set<ripple::uint256>> const& books,
    uint32_t ledgerSequence) const
{
    PgQuery pgQuery(pgPool_);
    std::stringstream booksBuffer;
    size_t numRows = 0;
    for (auto& book : books)
    {
        for (auto& offer : book.second)
        {
            booksBuffer << "\\\\x" << ripple::strHex(book.first) << '\t'
                        << std::to_string(ledgerSequence) << '\t' << "\\\\x"
                        << ripple::strHex(offer) << '\n';
            numRows++;
            // If the buffer gets too large, the insert fails. Not sure why. So
            // we insert after 1 million records
            if (numRows == 1000000)
            {
                pgQuery.bulkInsert("books", booksBuffer.str());
                booksBuffer.str("");
                numRows = 0;
            }
        }
    }
    if (numRows > 0)
    {
        pgQuery.bulkInsert("books", booksBuffer.str());
    }
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
                    // This is rather unelegant. For a deleted object, we don't
                    // know its type just from the key (or do we?). So, we just
                    // assume it is an offer and try to delete it. The
                    // alternative is to read the actual object out of the db
                    // from before it was deleted. This could result in a lot of
                    // individual reads though, so we chose to just delete
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
