#include <backend/BackendFactory.h>
#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <etl/NFTHelpers.h>
#include <main/Build.h>

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <cassandra.h>

#include <iostream>

static std::uint32_t const MAX_RETRIES = 5;
static std::chrono::seconds const WAIT_TIME = std::chrono::seconds(60);

static void
wait(boost::asio::steady_timer& timer, std::string const reason)
{
    BOOST_LOG_TRIVIAL(info) << reason << ". Waiting";
    timer.expires_after(WAIT_TIME);
    timer.wait();
    BOOST_LOG_TRIVIAL(info) << "Done";
}

static void
doNFTWrite(
    std::vector<NFTsData>& nfts,
    Backend::CassandraBackend& backend,
    std::string const tag)
{
    if (nfts.size() <= 0)
        return;
    auto const size = nfts.size();
    backend.writeNFTs(std::move(nfts));
    backend.sync();
    BOOST_LOG_TRIVIAL(info) << tag << ": Wrote " << size << " records";
}

static std::optional<Backend::TransactionAndMetadata>
doTryFetchTransaction(
    boost::asio::steady_timer& timer,
    Backend::CassandraBackend& backend,
    ripple::uint256 const& hash,
    boost::asio::yield_context& yield,
    std::uint32_t const attempts = 0)
{
    try
    {
        return backend.fetchTransaction(hash, yield);
    }
    catch (Backend::DatabaseTimeout const& e)
    {
        if (attempts >= MAX_RETRIES)
            throw e;

        wait(timer, "Transaction read error");
        return doTryFetchTransaction(timer, backend, hash, yield, attempts + 1);
    }
}

static Backend::LedgerPage
doTryFetchLedgerPage(
    boost::asio::steady_timer& timer,
    Backend::CassandraBackend& backend,
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield,
    std::uint32_t const attempts = 0)
{
    try
    {
        return backend.fetchLedgerPage(cursor, sequence, 2000, false, yield);
    }
    catch (Backend::DatabaseTimeout const& e)
    {
        if (attempts >= MAX_RETRIES)
            throw e;

        wait(timer, "Page read error");
        return doTryFetchLedgerPage(
            timer, backend, cursor, sequence, yield, attempts + 1);
    }
}

static const CassResult*
doTryGetTxPageResult(
    CassStatement* const query,
    boost::asio::steady_timer& timer,
    Backend::CassandraBackend& backend,
    std::uint32_t const attempts = 0)
{
    CassFuture* fut = cass_session_execute(backend.cautionGetSession(), query);
    CassResult const* result = cass_future_get_result(fut);
    cass_future_free(fut);

    if (result != nullptr)
        return result;

    if (attempts >= MAX_RETRIES)
        throw std::runtime_error("Already retried too many times");

    wait(timer, "Unexpected empty result from tx paging");
    return doTryGetTxPageResult(query, timer, backend, attempts + 1);
}

static void
doMigration(
    Backend::CassandraBackend& backend,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context& yield)
{
    BOOST_LOG_TRIVIAL(info) << "Beginning migration";
    auto const ledgerRange = backend.hardFetchLedgerRangeNoThrow(yield);

    /*
     * Step 0 - If we haven't downloaded the initial ledger yet, just short
     * circuit.
     */
    if (!ledgerRange)
    {
        BOOST_LOG_TRIVIAL(info) << "There is no data to migrate";
        return;
    }

    /*
     * Step 1 - Look at all NFT transactions recorded in
     * `nf_token_transactions` and reload any NFTokenMint transactions. These
     * will contain the URI of any tokens that were minted after our start
     * sequence. We look at transactions for this step instead of directly at
     * the tokens in `nf_tokens` because we also want to cover the extreme
     * edge case of a token that is re-minted with a different URI.
     */
    std::stringstream query;
    query << "SELECT hash FROM " << backend.tablePrefix()
          << "nf_token_transactions";
    CassStatement* nftTxQuery = cass_statement_new(query.str().c_str(), 0);
    cass_statement_set_paging_size(nftTxQuery, 1000);
    cass_bool_t morePages = cass_true;

    // For all NFT txs, paginated in groups of 1000...
    while (morePages)
    {
        std::vector<NFTsData> toWrite;

        CassResult const* result =
            doTryGetTxPageResult(nftTxQuery, timer, backend);

        // For each tx in page...
        CassIterator* txPageIterator = cass_iterator_from_result(result);
        while (cass_iterator_next(txPageIterator))
        {
            cass_byte_t const* buf;
            std::size_t bufSize;

            CassError const rc = cass_value_get_bytes(
                cass_row_get_column(cass_iterator_get_row(txPageIterator), 0),
                &buf,
                &bufSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(txPageIterator);
                cass_result_free(result);
                cass_statement_free(nftTxQuery);
                throw std::runtime_error(
                    "Could not retrieve hash from nf_token_transactions");
            }

            auto const txHash = ripple::uint256::fromVoid(buf);
            auto const tx =
                doTryFetchTransaction(timer, backend, txHash, yield);
            if (!tx)
            {
                cass_iterator_free(txPageIterator);
                cass_result_free(result);
                cass_statement_free(nftTxQuery);
                std::stringstream ss;
                ss << "Could not fetch tx with hash "
                   << ripple::to_string(txHash);
                throw std::runtime_error(ss.str());
            }

            // Not really sure how cassandra paging works, but we want to skip
            // any transactions that were loaded since the migration started
            if (tx->ledgerSequence > ledgerRange->maxSequence)
                continue;

            ripple::STTx const sttx{ripple::SerialIter{
                tx->transaction.data(), tx->transaction.size()}};
            if (sttx.getTxnType() != ripple::TxType::ttNFTOKEN_MINT)
                continue;

            ripple::TxMeta const txMeta{
                sttx.getTransactionID(), tx->ledgerSequence, tx->metadata};
            toWrite.push_back(
                std::get<1>(getNFTDataFromTx(txMeta, sttx)).value());
        }

        doNFTWrite(toWrite, backend, "TX");

        morePages = cass_result_has_more_pages(result);
        if (morePages)
            cass_statement_set_paging_state(nftTxQuery, result);
        cass_iterator_free(txPageIterator);
        cass_result_free(result);
    }

    cass_statement_free(nftTxQuery);
    BOOST_LOG_TRIVIAL(info) << "\nDone with transaction loading!\n";

    /*
     * Step 2 - Pull every object from our initial ledger and load all NFTs
     * found in any NFTokenPage object. Prior to this migration, we were not
     * pulling out NFTs from the initial ledger, so all these NFTs would be
     * missed. This will also record the URI of any NFTs minted prior to the
     * start sequence.
     */
    std::optional<ripple::uint256> cursor;

    do
    {
        auto const page = doTryFetchLedgerPage(
            timer, backend, cursor, ledgerRange->minSequence, yield);
        for (auto const& object : page.objects)
        {
            std::vector<NFTsData> toWrite = getNFTDataFromObj(
                ledgerRange->minSequence,
                ripple::to_string(object.key),
                std::string(object.blob.begin(), object.blob.end()));
            doNFTWrite(toWrite, backend, "OBJ");
        }
        cursor = page.cursor;
    } while (cursor.has_value());

    BOOST_LOG_TRIVIAL(info) << "\nDone with object loading!\n";

    /*
     * Step 3 - Drop the old `issuer_nf_tokens` table, which is replaced by
     * `issuer_nf_tokens_v2`. Normally, we should probably not drop old tables
     * in migrations, but here it is safe since the old table wasn't yet being
     * used to serve any data anyway.
     */
    query.str("");
    query << "DROP TABLE " << backend.tablePrefix() << "issuer_nf_tokens";
    CassStatement* issuerDropTableQuery =
        cass_statement_new(query.str().c_str(), 0);
    CassFuture* fut =
        cass_session_execute(backend.cautionGetSession(), issuerDropTableQuery);
    CassError const rc = cass_future_error_code(fut);
    cass_future_free(fut);
    cass_statement_free(issuerDropTableQuery);
    backend.sync();
    if (rc != CASS_OK)
        BOOST_LOG_TRIVIAL(warning) << "\nCould not drop old issuer_nf_tokens "
                                      "table. If it still exists, "
                                      "you should drop it yourself\n";
    else
        BOOST_LOG_TRIVIAL(info) << "\nDropped old 'issuer_nf_tokens' table!\n";

    BOOST_LOG_TRIVIAL(info)
        << "\nCompleted migration from " << ledgerRange->minSequence << " to "
        << ledgerRange->maxSequence << "!\n";
}

int
main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Didn't provide config path!" << std::endl;
        return EXIT_FAILURE;
    }

    std::string const configPath = argv[1];
    auto const config = clio::ConfigReader::open(configPath);
    if (!config)
    {
        std::cerr << "Couldn't parse config '" << configPath << "'"
                  << std::endl;
        return EXIT_FAILURE;
    }

    auto const type = config.value<std::string>("database.type");
    if (!boost::iequals(type, "cassandra"))
    {
        std::cerr << "Migration only for cassandra dbs" << std::endl;
        return EXIT_FAILURE;
    }

    boost::asio::io_context ioc;
    boost::asio::steady_timer timer{ioc};
    auto workGuard = boost::asio::make_work_guard(ioc);
    auto backend = Backend::make_Backend(ioc, config);

    boost::asio::spawn(
        ioc, [&backend, &workGuard, &timer](boost::asio::yield_context yield) {
            doMigration(*backend, timer, yield);
            workGuard.reset();
        });

    ioc.run();
    BOOST_LOG_TRIVIAL(info) << "SUCCESS!";
    return EXIT_SUCCESS;
}
