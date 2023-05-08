#include <backend/BackendFactory.h>
#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <etl/NFTHelpers.h>
#include <main/Build.h>

#include <rpc/Errors.h>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <cassandra.h>

#include <iostream>

static std::uint32_t const MAX_RETRIES = 5;
static std::chrono::seconds const WAIT_TIME = std::chrono::seconds(60);
using Blob = std::vector<unsigned char>;

static void
wait(boost::asio::steady_timer& timer, std::string const reason)
{
    BOOST_LOG_TRIVIAL(info) << reason << ". Waiting";
    timer.expires_after(WAIT_TIME);
    timer.wait();
    BOOST_LOG_TRIVIAL(info) << "Done";
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

    wait(timer, "Unexpected empty result from nft paging");
    return doTryGetTxPageResult(query, timer, backend, attempts + 1);
}

static void
verifyNFTs(
    std::vector<NFTsData>& nfts,
    Backend::CassandraBackend& backend,
    boost::asio::yield_context& yield)
{
    if (nfts.size() <= 0)
        return;

    for(auto const& nft: nfts){
        std::optional<Backend::NFT> writtenNFT = backend.fetchNFT(nft.tokenID, nft.ledgerSequence, yield);

        if(!writtenNFT.has_value())
            throw std::runtime_error("NFT was not written!");

        Blob writtenUriBlob = writtenNFT->uri;
        std::string writtenUriStr = ripple::strHex(writtenUriBlob);

        auto fetchOldUri = nft.uri;
        std::string oldUriStr = nft.uri.has_value() ? ripple::strHex(nft.uri.value()) : "";

        if(oldUriStr.compare(writtenUriStr) != 0){
            BOOST_LOG_TRIVIAL(warning) <<"\nNFTokenID "<< to_string(nft.tokenID) << " failed to match URIs!\n";  
            throw std::runtime_error("Failed to match!");
        }
        else{
            BOOST_LOG_TRIVIAL(info) <<"\nNFTokenID "<< to_string(nft.tokenID) << " URI matched!\n";         
        }
    }

}

static void
doVerification(
    Backend::CassandraBackend& backend,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context& yield)
{
    BOOST_LOG_TRIVIAL(info) << "Beginning verification";
    auto const ledgerRange = backend.hardFetchLedgerRangeNoThrow(yield);

    /*
     * If we haven't downloaded the initial ledger yet, just short
     * circuit.
     */
    if (!ledgerRange)
    {
        BOOST_LOG_TRIVIAL(info) << "There is no data to verification";
        return;
    }

    /*
     * Find all NFTokenPage objects and compare the URIs with what has been
     * written by the migrator
     */ 
    std::optional<ripple::uint256> cursor;
    do
    {
        auto const page = doTryFetchLedgerPage(
            timer, backend, cursor, ledgerRange->maxSequence, yield);
        for (auto const& object : page.objects)
        {
            std::vector<NFTsData> toVerify = getNFTDataFromObj(
                ledgerRange->maxSequence,
                std::string(object.key.begin(), object.key.end()),
                std::string(object.blob.begin(), object.blob.end()));

            //helper function to verify vector of NFTs
            verifyNFTs(toVerify, backend, yield);
        }
        cursor = page.cursor;
    } while (cursor.has_value());

    BOOST_LOG_TRIVIAL(info) << "\nDone with verification!\n";
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
            doVerification(*backend, timer, yield);
            workGuard.reset();
        });

    ioc.run();
    BOOST_LOG_TRIVIAL(info) << "SUCCESS!";
    return EXIT_SUCCESS;
}
