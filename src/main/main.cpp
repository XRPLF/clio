#include <backend/BackendFactory.h>
#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <etl/NFTHelpers.h>
#include <main/Build.h>
#include <rpc/RPCHelpers.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <cassandra.h>

#include <iostream>

static std::uint32_t const MAX_RETRIES = 5;
static std::chrono::seconds const WAIT_TIME = std::chrono::seconds(60);
static std::uint32_t const NFT_WRITE_BATCH_SIZE = 10000;

static void
wait(
    boost::asio::steady_timer& timer,
    std::string const& reason,
    std::chrono::seconds timeout = WAIT_TIME)
{
    clio::LogService::info() << reason << ". Waiting then retrying";
    timer.expires_after(timeout);
    timer.wait();
    clio::LogService::info() << "Done waiting";
}

static std::optional<boost::json::object>
doRequestFromRippled(
    clio::Config const& config,
    boost::json::object const& request)
{
    auto source = config.array("etl_sources").at(0);
    auto const ip = source.value<std::string>("ip");
    auto const wsPort = source.value<std::string>("ws_port");

    clio::LogService::debug()
        << "Attempting to forward request to tx. "
        << "request = " << boost::json::serialize(request);

    boost::json::object response;

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    using tcp = boost::asio::ip::tcp;

    try
    {
        boost::asio::io_context ioc;
        tcp::resolver resolver{ioc};

        auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(ioc);
        auto const results = resolver.resolve(ip, wsPort);

        ws->next_layer().expires_after(std::chrono::seconds(15));
        ws->next_layer().connect(results);

        ws->handshake(ip, "/");
        ws->write(net::buffer(boost::json::serialize(request)));

        beast::flat_buffer buffer;
        ws->read(buffer);

        auto begin = static_cast<char const*>(buffer.data().data());
        auto end = begin + buffer.data().size();
        auto parsed = boost::json::parse(std::string(begin, end));

        if (!parsed.is_object())
        {
            clio::LogService::error()
                << "Error parsing response: " << std::string{begin, end};
            return {};
        }

        return parsed.as_object();
    }
    catch (std::exception const& e)
    {
        clio::LogService::fatal() << "Encountered exception : " << e.what();
        return {};
    }
}

static std::optional<boost::json::object>
requestFromRippled(
    boost::asio::steady_timer& timer,
    clio::Config const& config,
    boost::json::object const& request,
    std::uint32_t const attempts = 0)
{
    auto response = doRequestFromRippled(config, request);
    if (response.has_value())
        return response;

    if (attempts >= MAX_RETRIES)
        return std::nullopt;

    wait(timer, "Failed to request from rippled", std::chrono::seconds{1});
    return requestFromRippled(timer, config, request, attempts + 1);
}

static std::string
hexStringToBinaryString(std::string hex)
{
    auto blob = ripple::strUnHex(hex);
    std::string strBlob;
    for (auto c : *blob)
        strBlob += c;
    return strBlob;
}

static void
maybeWriteTransaction(
    Backend::CassandraBackend& backend,
    std::optional<boost::json::object> const& tx)
{
    if (!tx.has_value())
        throw std::runtime_error("Could not repair transaction");

    auto package = tx.value();
    if (!package.contains("result") || !package.at("result").is_object() ||
        package.at("result").as_object().contains("error"))
        throw std::runtime_error("Received non-success response from rippled");

    auto data = package.at("result").as_object();

    auto const date = data.at("date").as_int64();
    auto const ledgerIndex = data.at("ledger_index").as_int64();
    auto hashStr = hexStringToBinaryString(data.at("hash").as_string().c_str());
    auto metaStr = hexStringToBinaryString(data.at("meta").as_string().c_str());
    auto txStr = hexStringToBinaryString(data.at("tx").as_string().c_str());

    backend.writeTransaction(
        std::move(hashStr),
        ledgerIndex,
        date,
        std::move(txStr),
        std::move(metaStr));
    backend.sync();
}

static void
repairCorruptedTx(
    boost::asio::steady_timer& timer,
    clio::Config const& config,
    Backend::CassandraBackend& backend,
    ripple::uint256 const& hash)
{
    clio::LogService::info() << " - repairing " << hash;
    auto const data = requestFromRippled(
        timer,
        config,
        {
            {"method", "tx"},
            {"transaction", to_string(hash)},
            {"binary", true},
        });

    maybeWriteTransaction(backend, data);
}

static std::vector<NFTsData>
doNFTWrite(
    std::vector<NFTsData>& nfts,
    Backend::CassandraBackend& backend,
    std::string const& tag)
{
    auto const size = nfts.size();
    if (size == 0)
        return nfts;
    backend.writeNFTs(std::move(nfts));
    backend.sync();
    clio::LogService::info() << tag << ": Wrote " << size << " records";
    return {};
}

static std::vector<NFTsData>
maybeDoNFTWrite(
    std::vector<NFTsData>& nfts,
    Backend::CassandraBackend& backend,
    std::string const& tag)
{
    if (nfts.size() < NFT_WRITE_BATCH_SIZE)
        return nfts;
    return doNFTWrite(nfts, backend, tag);
}

static std::vector<Backend::TransactionAndMetadata>
doTryFetchTransactions(
    boost::asio::steady_timer& timer,
    Backend::CassandraBackend& backend,
    std::vector<ripple::uint256> const& hashes,
    boost::asio::yield_context& yield,
    std::uint32_t const attempts = 0)
{
    try
    {
        return backend.fetchTransactions(hashes, yield);
    }
    catch (Backend::DatabaseTimeout const& e)
    {
        if (attempts >= MAX_RETRIES)
            throw e;

        wait(timer, "Transactions read error");
        return doTryFetchTransactions(
            timer, backend, hashes, yield, attempts + 1);
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
        return backend.fetchLedgerPage(cursor, sequence, 10000, false, yield);
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
doMigrationStepOne(
    clio::Config const& config,
    Backend::CassandraBackend& backend,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context& yield,
    Backend::LedgerRange const& ledgerRange,
    bool repairEnabled = false)
{
    /*
     * Step 1 - Look at all NFT transactions recorded in
     * `nf_token_transactions` and reload any NFTokenMint transactions. These
     * will contain the URI of any tokens that were minted after our start
     * sequence. We look at transactions for this step instead of directly at
     * the tokens in `nf_tokens` because we also want to cover the extreme
     * edge case of a token that is re-minted with a different URI.
     */
    std::string const stepTag = "Step 1 - transaction loading";
    std::vector<NFTsData> toWrite;

    std::stringstream query;
    query << "SELECT hash FROM " << backend.tablePrefix()
          << "nf_token_transactions";
    CassStatement* nftTxQuery = cass_statement_new(query.str().c_str(), 0);
    cass_statement_set_paging_size(nftTxQuery, 1000);
    cass_bool_t morePages = cass_true;

    // For all NFT txs, paginated in groups of 1000...
    while (morePages)
    {
        CassResult const* result =
            doTryGetTxPageResult(nftTxQuery, timer, backend);

        std::vector<ripple::uint256> txHashes;

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

            txHashes.push_back(ripple::uint256::fromVoid(buf));
        }

        auto txs = doTryFetchTransactions(timer, backend, txHashes, yield);
        if (txs.size() != txHashes.size())
            throw std::runtime_error(
                "Amount of hashes does not match amount of retrieved "
                "transactions");

        for (int32_t idx = 0; idx < txHashes.size(); ++idx)
        {
            auto const& tx = txs.at(idx);
            auto const& hash = txHashes.at(idx);

            if (tx.ledgerSequence > ledgerRange.maxSequence)
                continue;

            try
            {
                ripple::STTx const sttx{ripple::SerialIter{
                    tx.transaction.data(), tx.transaction.size()}};
                if (sttx.getTxnType() != ripple::TxType::ttNFTOKEN_MINT)
                    continue;

                ripple::TxMeta const txMeta{
                    sttx.getTransactionID(), tx.ledgerSequence, tx.metadata};
                toWrite.push_back(
                    std::get<1>(getNFTDataFromTx(txMeta, sttx)).value());
            }
            catch (std::exception const& e)
            {
                clio::LogService::warn() << "Corrupted tx detected: " << hash;
                std::cerr << "Corrupted tx detected: " << hash << std::endl;

                if (not repairEnabled)
                {
                    clio::LogService::fatal()
                        << "Not attempting to repair. Rerun with -repair to "
                           "repair corrupted transactions.";
                    exit(-1);
                }

                repairCorruptedTx(timer, config, backend, hash);

                auto maybeTx = backend.fetchTransaction(hash, yield);

                if (!maybeTx.has_value())
                {
                    clio::LogService::fatal()
                        << "Could not fetch written transaction for hash "
                        << hash << "; Repair failed.";
                    exit(-1);
                }

                txs[idx] = maybeTx.value();
                --idx;  // repeat the try section for the repaired tx
                std::cerr << "+ tx repaired: " << hash << std::endl;
            }
        }

        toWrite = maybeDoNFTWrite(toWrite, backend, stepTag);

        morePages = cass_result_has_more_pages(result);
        if (morePages)
            cass_statement_set_paging_state(nftTxQuery, result);
        cass_iterator_free(txPageIterator);
        cass_result_free(result);
    }

    cass_statement_free(nftTxQuery);
    doNFTWrite(toWrite, backend, stepTag);
}

static void
doMigrationStepTwo(
    Backend::CassandraBackend& backend,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context& yield,
    Backend::LedgerRange const& ledgerRange)
{
    /*
     * Step 2 - Pull every object from our initial ledger and load all NFTs
     * found in any NFTokenPage object. Prior to this migration, we were not
     * pulling out NFTs from the initial ledger, so all these NFTs would be
     * missed. This will also record the URI of any NFTs minted prior to the
     * start sequence.
     */
    std::string const stepTag = "Step 2 - initial ledger loading";
    std::vector<NFTsData> toWrite;
    std::optional<ripple::uint256> cursor;

    // For each object page in initial ledger
    do
    {
        auto const page = doTryFetchLedgerPage(
            timer, backend, cursor, ledgerRange.minSequence, yield);

        // For each object in page
        for (auto const& object : page.objects)
        {
            auto const objectNFTs = getNFTDataFromObj(
                ledgerRange.minSequence,
                std::string(object.key.begin(), object.key.end()),
                std::string(object.blob.begin(), object.blob.end()));
            toWrite.insert(toWrite.end(), objectNFTs.begin(), objectNFTs.end());
        }

        toWrite = maybeDoNFTWrite(toWrite, backend, stepTag);
        cursor = page.cursor;
    } while (cursor.has_value());

    doNFTWrite(toWrite, backend, stepTag);
}

static void
doMigrationStepThree(Backend::CassandraBackend& backend)
{
    /*
     * Step 3 - Drop the old `issuer_nf_tokens` table, which is replaced by
     * `issuer_nf_tokens_v2`. Normally, we should probably not drop old tables
     * in migrations, but here it is safe since the old table wasn't yet being
     * used to serve any data anyway.
     */
    std::stringstream query;
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
        clio::LogService::warn() << "Could not drop old issuer_nf_tokens "
                                    "table. If it still exists, "
                                    "you should drop it yourself\n";
}

static void
doMigration(
    clio::Config const& config,
    Backend::CassandraBackend& backend,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context& yield,
    bool repairEnabled = false)
{
    clio::LogService::info() << "Beginning migration";
    auto const ledgerRange = backend.hardFetchLedgerRangeNoThrow(yield);

    /*
     * Step 0 - If we haven't downloaded the initial ledger yet, just short
     * circuit.
     */
    if (!ledgerRange)
    {
        clio::LogService::info() << "There is no data to migrate";
        return;
    }

    doMigrationStepOne(
        config, backend, timer, yield, *ledgerRange, repairEnabled);
    clio::LogService::info() << "\nStep 1 done!\n";

    doMigrationStepTwo(backend, timer, yield, *ledgerRange);
    clio::LogService::info() << "\nStep 2 done!\n";

    doMigrationStepThree(backend);
    clio::LogService::info() << "\nStep 3 done!\n";

    clio::LogService::info()
        << "\nCompleted migration from " << ledgerRange->minSequence << " to "
        << ledgerRange->maxSequence << "!\n";
}

static void
usage()
{
    std::cerr << "\nUsage:\n"
              << "    with repair: clio_migrator path/to/config -repair 2> "
                 "repair.log\n"
              << " without repair: clio_migrator path/to/config" << std::endl;
}

int
main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Didn't provide config path." << std::endl;
        usage();
        return EXIT_FAILURE;
    }

    auto repairEnabled = false;
    if (argc >= 3)
    {
        if (not boost::iequals(argv[2], "-repair"))
        {
            std::cerr << "Final argument must be `-repair`." << std::endl;
            usage();
            return EXIT_FAILURE;
        }
        clio::LogService::info()
            << "Enabling REPAIR mode. Missing/broken transactions will be "
               "downloaded from rippled and overwritten.";
        repairEnabled = true;
    }

    std::string const configPath = argv[1];
    auto const config = clio::ConfigReader::open(configPath);
    if (!config)
    {
        std::cerr << "Couldn't parse config '" << configPath << "'"
                  << std::endl;
        return EXIT_FAILURE;
    }

    clio::LogService::init(config);

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
        ioc,
        [&config, &backend, &workGuard, &timer, &repairEnabled](
            boost::asio::yield_context yield) {
            doMigration(config, *backend, timer, yield, repairEnabled);
            workGuard.reset();
        });

    ioc.run();
    clio::LogService::info() << "SUCCESS!";
    return EXIT_SUCCESS;
}
