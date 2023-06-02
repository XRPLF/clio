//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <etl/ETLService.h>

using namespace clio;

// Database must be populated when this starts
std::optional<uint32_t>
ETLService::runETLPipeline(uint32_t startSequence, int numExtractors)
{
    if (finishSequence_ && startSequence > *finishSequence_)
        return {};

    log_.debug() << "Starting etl pipeline";
    state_.isWriting = true;

    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (!rng || rng->maxSequence < startSequence - 1)
    {
        assert(false);
        throw std::runtime_error("runETLPipeline: parent ledger is null");
    }

    auto const begin = std::chrono::system_clock::now();
    auto extractors = std::vector<std::unique_ptr<ExtractorType>>{};
    auto pipe = DataPipeType{numExtractors, startSequence};

    for (auto i = 0u; i < numExtractors; ++i)
        extractors.push_back(std::make_unique<ExtractorType>(
            pipe, networkValidatedLedgers_, ledgerFetcher_, startSequence + i, finishSequence_, state_));

    auto transformer = TransformerType{pipe, backend_, ledgerLoader_, ledgerPublisher_, startSequence, state_};
    transformer.waitTillFinished();  // suspend current thread until exit condition is met
    pipe.cleanup();                  // TODO: this should probably happen automatically using destructor

    // wait for all of the extractors to stop
    for (auto& t : extractors)
        t->waitTillFinished();

    auto const end = std::chrono::system_clock::now();
    auto const lastPublishedSeq = ledgerPublisher_.getLastPublishedSequence();
    log_.debug() << "Extracted and wrote " << lastPublishedSeq.value_or(startSequence) - startSequence << " in "
                 << ((end - begin).count()) / 1000000000.0;

    state_.isWriting = false;

    log_.debug() << "Stopping etl pipeline";
    return lastPublishedSeq;
}

// Main loop of ETL.
// The software begins monitoring the ledgers that are validated by the nework.
// The member networkValidatedLedgers_ keeps track of the sequences of ledgers validated by the network.
// Whenever a ledger is validated by the network, the software looks for that ledger in the database. Once the ledger is
// found in the database, the software publishes that ledger to the ledgers stream. If a network validated ledger is not
// found in the database after a certain amount of time, then the software attempts to take over responsibility of the
// ETL process, where it writes new ledgers to the database. The software will relinquish control of the ETL process if
// it detects that another process has taken over ETL.
void
ETLService::monitor()
{
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (!rng)
    {
        log_.info() << "Database is empty. Will download a ledger "
                       "from the network.";
        std::optional<ripple::LedgerInfo> ledger;

        if (startSequence_)
        {
            log_.info() << "ledger sequence specified in config. "
                        << "Will begin ETL process starting with ledger " << *startSequence_;
            ledger = ledgerLoader_.loadInitialLedger(*startSequence_);
        }
        else
        {
            log_.info() << "Waiting for next ledger to be validated by network...";
            std::optional<uint32_t> mostRecentValidated = networkValidatedLedgers_->getMostRecent();

            if (mostRecentValidated)
            {
                log_.info() << "Ledger " << *mostRecentValidated << " has been validated. "
                            << "Downloading...";
                ledger = ledgerLoader_.loadInitialLedger(*mostRecentValidated);
            }
            else
            {
                log_.info() << "The wait for the next validated "
                            << "ledger has been aborted. "
                            << "Exiting monitor loop";
                return;
            }
        }

        if (ledger)
        {
            rng = backend_->hardFetchLedgerRangeNoThrow();
        }
        else
        {
            log_.error() << "Failed to load initial ledger. Exiting monitor loop";
            return;
        }
    }
    else
    {
        if (startSequence_)
            log_.warn() << "start sequence specified but db is already populated";

        log_.info() << "Database already populated. Picking up from the tip of history";
        cacheLoader_.load(rng->maxSequence);
    }

    assert(rng);
    uint32_t nextSequence = rng->maxSequence + 1;

    log_.debug() << "Database is populated. "
                 << "Starting monitor loop. sequence = " << nextSequence;

    while (true)
    {
        if (auto rng = backend_->hardFetchLedgerRangeNoThrow(); rng && rng->maxSequence >= nextSequence)
        {
            ledgerPublisher_.publish(nextSequence, {});
            ++nextSequence;
        }
        else if (networkValidatedLedgers_->waitUntilValidatedByNetwork(nextSequence, 1000))
        {
            log_.info() << "Ledger with sequence = " << nextSequence << " has been validated by the network. "
                        << "Attempting to find in database and publish";

            // Attempt to take over responsibility of ETL writer after 10 failed
            // attempts to publish the ledger. publishLedger() fails if the
            // ledger that has been validated by the network is not found in the
            // database after the specified number of attempts. publishLedger()
            // waits one second between each attempt to read the ledger from the
            // database
            constexpr size_t timeoutSeconds = 10;
            bool success = ledgerPublisher_.publish(nextSequence, timeoutSeconds);

            if (!success)
            {
                log_.warn() << "Failed to publish ledger with sequence = " << nextSequence << " . Beginning ETL";

                // returns the most recent sequence published empty optional if no sequence was published
                std::optional<uint32_t> lastPublished = runETLPipeline(nextSequence, extractorThreads_);
                log_.info() << "Aborting ETL. Falling back to publishing";

                // if no ledger was published, don't increment nextSequence
                if (lastPublished)
                    nextSequence = *lastPublished + 1;
            }
            else
            {
                ++nextSequence;
            }
        }
    }
}

void
ETLService::monitorReadOnly()
{
    log_.debug() << "Starting reporting in strict read only mode";

    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    uint32_t latestSequence;

    if (!rng)
    {
        if (auto net = networkValidatedLedgers_->getMostRecent())
            latestSequence = *net;
        else
            return;
    }
    else
    {
        latestSequence = rng->maxSequence;
    }

    cacheLoader_.load(latestSequence);
    latestSequence++;

    while (true)
    {
        if (auto rng = backend_->hardFetchLedgerRangeNoThrow(); rng && rng->maxSequence >= latestSequence)
        {
            ledgerPublisher_.publish(latestSequence, {});
            latestSequence = latestSequence + 1;
        }
        else
        {
            // if we can't, wait until it's validated by the network, or 1 second passes, whichever occurs first.
            // Even if we don't hear from rippled, if ledgers are being written to the db, we publish them.
            networkValidatedLedgers_->waitUntilValidatedByNetwork(latestSequence, 1000);
        }
    }
}

void
ETLService::run()
{
    log_.info() << "Starting reporting etl";
    state_.isStopping = false;

    doWork();
}

void
ETLService::doWork()
{
    worker_ = std::thread([this]() {
        beast::setCurrentThreadName("rippled: ETLService worker");

        if (state_.isReadOnly)
            monitorReadOnly();
        else
            monitor();
    });
}

ETLService::ETLService(
    clio::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManagerType> subscriptions,
    std::shared_ptr<LoadBalancerType> balancer,
    std::shared_ptr<NetworkValidatedLedgersType> ledgers)
    : backend_(backend)
    , loadBalancer_(balancer)
    , networkValidatedLedgers_(ledgers)
    , cacheLoader_(config, ioc, backend, backend->cache())
    , ledgerFetcher_(backend, balancer)
    , ledgerLoader_(backend, balancer, ledgerFetcher_, state_)
    , ledgerPublisher_(ioc, backend, subscriptions, state_)
{
    startSequence_ = config.maybeValue<uint32_t>("start_sequence");
    finishSequence_ = config.maybeValue<uint32_t>("finish_sequence");
    state_.isReadOnly = config.valueOr("read_only", state_.isReadOnly);
    extractorThreads_ = config.valueOr<uint32_t>("extractor_threads", extractorThreads_);
    txnThreshold_ = config.valueOr<size_t>("txn_threshold", txnThreshold_);
}
