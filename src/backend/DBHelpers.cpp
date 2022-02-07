#include <boost/format.hpp>
#include <backend/DBHelpers.h>
#include <memory>

// static bool
// writeToLedgersDB(boost::asio::yield_context yield, ripple::LedgerInfo const&
// info, PgQuery& pgQuery)
// {
//     BOOST_LOG_TRIVIAL(debug) << __func__;
//     auto cmd = boost::format(
//         R"(INSERT INTO ledgers
//            VALUES (%u,'\x%s', '\x%s',%u,%u,%u,%u,%u,'\x%s','\x%s'))");

//     auto ledgerInsert = boost::str(
//         cmd % info.seq % ripple::strHex(info.hash) %
//         ripple::strHex(info.parentHash) % info.drops.drops() %
//         info.closeTime.time_since_epoch().count() %
//         info.parentCloseTime.time_since_epoch().count() %
//         info.closeTimeResolution.count() % info.closeFlags %
//         ripple::strHex(info.accountHash) % ripple::strHex(info.txHash));
//     BOOST_LOG_TRIVIAL(trace) << __func__ << " : "
//                              << " : "
//                              << "query string = " << ledgerInsert;

//     auto res = pgQuery(yield, ledgerInsert.data());

//     return res;
// }

/*
bool
writeBooks(std::vector<BookDirectoryData> const& bookDirData, PgQuery& pg)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Writing " << bookDirData.size() << "books to Postgres";

    try
    {
        std::stringstream booksCopyBuffer;
        for (auto const& data : bookDirData)
        {
            std::string directoryIndex = ripple::strHex(data.directoryIndex);
            std::string bookIndex = ripple::strHex(data.bookIndex);
            auto ledgerSeq = data.ledgerSequence;

            booksCopyBuffer << "\\\\x" << directoryIndex << '\t'
                            << std::to_string(ledgerSeq) << '\t' << "\\\\x"
                            << bookIndex << '\n';
        }

        pg.bulkInsert("books", booksCopyBuffer.str());

        BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                << "Successfully inserted  books";
        return true;
    }
    catch (std::exception& e)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << "Caught exception inserting books : " << e.what();
        assert(false);
        return false;
    }
}
*/

/*
bool
writeToPostgres(
    ripple::LedgerInfo const& info,
    std::vector<AccountTransactionsData> const& accountTxData,
    std::shared_ptr<PgPool> const& pgPool)
{
    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "Beginning write to Postgres";

    try
    {
        // Create a PgQuery object to run multiple commands over the
        // same connection in a single transaction block.
        PgQuery pg(pgPool);
        auto res = pg("BEGIN");
        if (!res || res.status() != PGRES_COMMAND_OK)
        {
            std::stringstream msg;
            msg << "bulkWriteToTable : Postgres insert error: " << res.msg();
            throw std::runtime_error(msg.str());
        }

        // Writing to the ledgers db fails if the ledger already
        // exists in the db. In this situation, the ETL process has
        // detected there is another writer, and falls back to only
        // publishing
        if (!writeToLedgersDB(info, pg))
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " : "
                << "Failed to write to ledgers database.";
            return false;
        }

        std::stringstream transactionsCopyBuffer;
        std::stringstream accountTransactionsCopyBuffer;
        for (auto const& data : accountTxData)
        {
            std::string txHash = ripple::strHex(data.txHash);
            std::string nodestoreHash = ripple::strHex(data.nodestoreHash);
            auto idx = data.transactionIndex;
            auto ledgerSeq = data.ledgerSequence;

            transactionsCopyBuffer << std::to_string(ledgerSeq) << '\t'
                                   << std::to_string(idx) << '\t' << "\\\\x"
                                   << txHash << '\t' << "\\\\x" << nodestoreHash
                                   << '\n';

            for (auto const& a : data.accounts)
            {
                std::string acct = ripple::strHex(a);
                accountTransactionsCopyBuffer
                    << "\\\\x" << acct << '\t' << std::to_string(ledgerSeq)
                    << '\t' << std::to_string(idx) << '\n';
            }
        }

        pg.bulkInsert("transactions", transactionsCopyBuffer.str());
        pg.bulkInsert(
            "account_transactions", accountTransactionsCopyBuffer.str());

        res = pg("COMMIT");
        if (!res || res.status() != PGRES_COMMAND_OK)
        {
            std::stringstream msg;
            msg << "bulkWriteToTable : Postgres insert error: " << res.msg();
            assert(false);
            throw std::runtime_error(msg.str());
        }

        BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                << "Successfully wrote to Postgres";
        return true;
    }
    catch (std::exception& e)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__
            << "Caught exception writing to Postgres : " << e.what();
        assert(false);
        return false;
    }
}
*/
