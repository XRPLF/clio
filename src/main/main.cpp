#include <backend/BackendFactory.h>
#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <etl/NFTHelpers.h>
#include <main/Build.h>
#include <main/migration/Helpers.h>
#include <main/migration/Migrations.h>
#include <rpc/RPCHelpers.h>

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <cassandra.h>

#include <iostream>

class Step1Impl
{
    std::string tag_;
    std::reference_wrapper<clio::Config const> config_;
    std::shared_ptr<Backend::CassandraBackend> backend_;
    std::reference_wrapper<ResumeContextProvider> resumeProvider_;
    boost::json::object resumeData_;

    boost::asio::steady_timer timer_;
    bool repairEnabled_ = false;

public:
    Step1Impl(
        std::string tag,
        boost::asio::io_context& ioc,
        clio::Config const& config,
        std::shared_ptr<Backend::CassandraBackend> backend,
        ResumeContextProvider& resumeProvider,
        boost::json::object resumeData,
        bool repairEnabled)
        : tag_{std::move(tag)}
        , config_{std::cref(config)}
        , backend_{backend}
        , resumeProvider_{std::ref(resumeProvider)}
        , resumeData_{std::move(resumeData)}
        , timer_{ioc}
        , repairEnabled_{repairEnabled}
    {
    }

    void
    perform(
        boost::asio::yield_context yield,
        Backend::LedgerRange const& ledgerRange)
    {
        /*
         * Step 1 - Look at all NFT transactions recorded in
         * `nf_token_transactions` and reload any NFTokenMint transactions.
         * These will contain the URI of any tokens that were minted after our
         * start sequence. We look at transactions for this step instead of
         * directly at the tokens in `nf_tokens` because we also want to cover
         * the extreme edge case of a token that is re-minted with a different
         * URI.
         */
        std::vector<NFTsData> toWrite;

        clio::LogService::info() << "Running " << tag_;
        resumeProvider_.get().write({tag_, {}});  // at the start of step1

        std::stringstream query;
        query << "SELECT hash FROM " << backend_->tablePrefix()
              << "nf_token_transactions";
        CassStatement* nftTxQuery = cass_statement_new(query.str().c_str(), 0);
        cass_statement_set_paging_size(nftTxQuery, 1000);
        cass_bool_t morePages = cass_true;

        if (not resumeData_.empty() and resumeData_.contains("token") and
            resumeData_.at("token").is_string() and
            not resumeData_.at("token").as_string().empty())
        {
            clio::LogService::info() << " -- Restoring previous state..";

            auto encodedState =
                std::string{resumeData_.at("token").as_string().c_str()};
            auto state = ripple::base64_decode(encodedState);

            cass_statement_set_paging_state_token(
                nftTxQuery, state.c_str(), state.size());

            clio::LogService::info()
                << "    Resuming from page " << encodedState;
        }

        // For all NFT txs, paginated in groups of 1000...
        while (morePages)
        {
            CassResult const* result =
                doTryGetTxPageResult(nftTxQuery, timer_, backend_);

            std::vector<ripple::uint256> txHashes;

            // For each tx in page...
            CassIterator* txPageIterator = cass_iterator_from_result(result);
            while (cass_iterator_next(txPageIterator))
            {
                cass_byte_t const* buf;
                std::size_t bufSize;

                CassError const rc = cass_value_get_bytes(
                    cass_row_get_column(
                        cass_iterator_get_row(txPageIterator), 0),
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

            auto txs =
                doTryFetchTransactions(timer_, backend_, txHashes, yield);
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
                        sttx.getTransactionID(),
                        tx.ledgerSequence,
                        tx.metadata};
                    toWrite.push_back(
                        std::get<1>(getNFTDataFromTx(txMeta, sttx)).value());
                }
                catch (std::exception const& e)
                {
                    clio::LogService::warn()
                        << "Corrupted tx detected: " << hash;
                    std::cerr << "Corrupted tx detected: " << hash << std::endl;

                    if (not repairEnabled_)
                    {
                        clio::LogService::fatal()
                            << "Not attempting to repair. Rerun with -repair "
                               "to repair corrupted transactions.";
                        exit(-1);
                    }

                    repairCorruptedTx(timer_, config_.get(), backend_, hash);

                    auto maybeTx = backend_->fetchTransaction(hash, yield);

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

            toWrite = maybeDoNFTWrite(toWrite, backend_, tag_);

            morePages = cass_result_has_more_pages(result);
            if (morePages)
            {
                char const* state = nullptr;
                std::size_t sz;
                cass_result_paging_state_token(result, &state, &sz);
                cass_statement_set_paging_state_token(nftTxQuery, state, sz);

                resumeProvider_.get().write(
                    {tag_,
                     {{"token",
                       ripple::base64_encode(std::string{state, sz})}}});
            }

            cass_iterator_free(txPageIterator);
            cass_result_free(result);
        }

        cass_statement_free(nftTxQuery);
        doNFTWrite(toWrite, backend_, tag_);
    }
};

class Step2Impl
{
    std::string tag_;
    std::shared_ptr<Backend::CassandraBackend> backend_;
    std::reference_wrapper<ResumeContextProvider> resumeProvider_;
    boost::json::object resumeData_;

    boost::asio::steady_timer timer_;

public:
    Step2Impl(
        std::string tag,
        boost::asio::io_context& ioc,
        std::shared_ptr<Backend::CassandraBackend> backend,
        ResumeContextProvider& resumeProvider,
        boost::json::object resumeData)
        : tag_{std::move(tag)}
        , backend_{backend}
        , resumeProvider_{std::ref(resumeProvider)}
        , resumeData_{std::move(resumeData)}
        , timer_{ioc}
    {
    }

    void
    perform(
        boost::asio::yield_context yield,
        Backend::LedgerRange const& ledgerRange)
    {
        /*
         * Step 2 - Pull every object from our initial ledger and load all NFTs
         * found in any NFTokenPage object. Prior to this migration, we were not
         * pulling out NFTs from the initial ledger, so all these NFTs would be
         * missed. This will also record the URI of any NFTs minted prior to the
         * start sequence.
         */
        std::vector<NFTsData> toWrite;
        std::optional<ripple::uint256> cursor;

        clio::LogService::info() << "Running " << tag_;
        resumeProvider_.get().write({tag_, {}});  // at the start of step2

        if (not resumeData_.empty() and resumeData_.contains("cursor") and
            resumeData_.at("cursor").is_string() and
            not resumeData_.at("cursor").as_string().empty())
        {
            clio::LogService::info() << " -- Restoring previous state..";
            cursor =
                ripple::strUnHex(resumeData_.at("cursor").as_string().c_str());
            clio::LogService::info() << "    Resuming from " << *cursor;
        }

        // For each object page in initial ledger
        do
        {
            auto const page = doTryFetchLedgerPage(
                timer_, backend_, cursor, ledgerRange.minSequence, yield);

            // For each object in page
            for (auto const& object : page.objects)
            {
                auto const objectNFTs = getNFTDataFromObj(
                    ledgerRange.minSequence,
                    std::string(object.key.begin(), object.key.end()),
                    std::string(object.blob.begin(), object.blob.end()));
                toWrite.insert(
                    toWrite.end(), objectNFTs.begin(), objectNFTs.end());
            }

            toWrite = maybeDoNFTWrite(toWrite, backend_, tag_);
            cursor = page.cursor;

            if (cursor.has_value())
                resumeProvider_.get().write(
                    {tag_, {{"cursor", std::string{ripple::strHex(*cursor)}}}});

        } while (cursor.has_value());

        doNFTWrite(toWrite, backend_, tag_);
    }
};

class Step3Impl
{
    std::string tag_;
    std::shared_ptr<Backend::CassandraBackend> backend_;

public:
    Step3Impl(
        std::string tag,
        std::shared_ptr<Backend::CassandraBackend> backend)
        : tag_{std::move(tag)}, backend_{backend}
    {
    }

    void
    perform()
    {
        /*
         * Step 3 - Drop the old `issuer_nf_tokens` table, which is replaced by
         * `issuer_nf_tokens_v2`. Normally, we should probably not drop old
         * tables in migrations, but here it is safe since the old table wasn't
         * yet being used to serve any data anyway.
         */
        clio::LogService::info() << "Running " << tag_;

        std::stringstream query;
        query << "DROP TABLE " << backend_->tablePrefix() << "issuer_nf_tokens";
        CassStatement* issuerDropTableQuery =
            cass_statement_new(query.str().c_str(), 0);
        CassFuture* fut = cass_session_execute(
            backend_->cautionGetSession(), issuerDropTableQuery);
        CassError const rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(issuerDropTableQuery);
        backend_->sync();

        if (rc != CASS_OK)
            clio::LogService::warn() << "Could not drop old issuer_nf_tokens "
                                        "table. If it still exists, "
                                        "you should drop it yourself\n";
    }
};

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
    auto backend = Backend::make_Backend(ioc, config);
    auto resumeProvider =
        ResumeContextProvider(std::filesystem::current_path() / "resume.json");
    auto migrator = Migrator{
        ioc,
        config,
        backend,
        resumeProvider,
        {
            Step(
                "Step 1 - transaction loading",
                [&](auto tag,
                    auto yield,
                    auto const& ledgerRange,
                    auto resumeData) {
                    Step1Impl(
                        tag,
                        ioc,
                        config,
                        backend,
                        resumeProvider,
                        resumeData,
                        repairEnabled)
                        .perform(yield, ledgerRange);
                }),
            Step(
                "Step 2 - initial ledger loading",
                [&](auto tag,
                    auto yield,
                    auto const& ledgerRange,
                    auto resumeData) {
                    Step2Impl(tag, ioc, backend, resumeProvider, resumeData)
                        .perform(yield, ledgerRange);
                }),
            Step(
                "Step 3 - cleanup",
                [&](auto tag,
                    auto yield,
                    auto const& ledgerRange,
                    auto resumeData) { Step3Impl(tag, backend).perform(); }),
        }};

    ioc.run();

    clio::LogService::info() << "SUCCESS!";
    return EXIT_SUCCESS;
}
