//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etlng/ETLService.hpp"

#include "data/BackendInterface.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/ETLState.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etlng/CacheLoaderInterface.hpp"
#include "etlng/CacheUpdaterInterface.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/LedgerPublisherInterface.hpp"
#include "etlng/LoadBalancerInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/MonitorInterface.hpp"
#include "etlng/MonitorProviderInterface.hpp"
#include "etlng/TaskManagerProviderInterface.hpp"
#include "etlng/impl/AmendmentBlockHandler.hpp"
#include "etlng/impl/CacheUpdater.hpp"
#include "etlng/impl/Extraction.hpp"
#include "etlng/impl/LedgerPublisher.hpp"
#include "etlng/impl/Loading.hpp"
#include "etlng/impl/Registry.hpp"
#include "etlng/impl/Scheduling.hpp"
#include "etlng/impl/TaskManager.hpp"
#include "etlng/impl/ext/Cache.hpp"
#include "etlng/impl/ext/Core.hpp"
#include "etlng/impl/ext/NFT.hpp"
#include "etlng/impl/ext/Successor.hpp"
#include "util/Assert.hpp"
#include "util/Profiler.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <boost/signals2/connection.hpp>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace etlng {

ETLService::ETLService(
    util::async::AnyExecutionContext ctx,
    std::reference_wrapper<util::config::ClioConfigDefinition const> config,
    std::shared_ptr<data::BackendInterface> backend,
    std::shared_ptr<LoadBalancerInterface> balancer,
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> ledgers,
    std::shared_ptr<LedgerPublisherInterface> publisher,
    std::shared_ptr<CacheLoaderInterface> cacheLoader,
    std::shared_ptr<CacheUpdaterInterface> cacheUpdater,
    std::shared_ptr<ExtractorInterface> extractor,
    std::shared_ptr<LoaderInterface> loader,
    std::shared_ptr<InitialLoadObserverInterface> initialLoadObserver,
    std::shared_ptr<etlng::TaskManagerProviderInterface> taskManagerProvider,
    std::shared_ptr<etlng::MonitorProviderInterface> monitorProvider,
    std::shared_ptr<etl::SystemState> state
)
    : ctx_(std::move(ctx))
    , config_(config)
    , backend_(std::move(backend))
    , balancer_(std::move(balancer))
    , ledgers_(std::move(ledgers))
    , publisher_(std::move(publisher))
    , cacheLoader_(std::move(cacheLoader))
    , cacheUpdater_(std::move(cacheUpdater))
    , extractor_(std::move(extractor))
    , loader_(std::move(loader))
    , initialLoadObserver_(std::move(initialLoadObserver))
    , taskManagerProvider_(std::move(taskManagerProvider))
    , monitorProvider_(std::move(monitorProvider))
    , state_(std::move(state))
    , startSequence_(config.get().maybeValue<uint32_t>("start_sequence"))
    , finishSequence_(config.get().maybeValue<uint32_t>("finish_sequence"))
{
    ASSERT(not state_->isWriting, "ETL should never start in writer mode");

    if (startSequence_.has_value())
        LOG(log_.info()) << "Start sequence: " << *startSequence_;

    if (finishSequence_.has_value())
        LOG(log_.info()) << "Finish sequence: " << *finishSequence_;

    LOG(log_.info()) << "Starting in " << (state_->isStrictReadonly ? "STRICT READONLY MODE" : "WRITE MODE");
}

ETLService::~ETLService()
{
    stop();
    LOG(log_.debug()) << "Destroying ETLng";
}

void
ETLService::run()
{
    LOG(log_.info()) << "Running ETLng...";

    mainLoop_.emplace(ctx_.execute([this] {
        auto const rng = loadInitialLedgerIfNeeded();

        LOG(log_.info()) << "Waiting for next ledger to be validated by network...";
        std::optional<uint32_t> const mostRecentValidated = ledgers_->getMostRecent();

        if (not mostRecentValidated) {
            LOG(log_.info()) << "The wait for the next validated ledger has been aborted. "
                                "Exiting monitor loop";
            return;
        }

        if (not rng.has_value()) {
            LOG(log_.warn()) << "Initial ledger download got cancelled - stopping ETL service";
            return;
        }

        auto const nextSequence = rng->maxSequence + 1;

        LOG(log_.debug()) << "Database is populated. Starting monitor loop. sequence = " << nextSequence;
        startMonitor(nextSequence);

        // If we are a writer as the result of loading the initial ledger - start loading
        if (state_->isWriting)
            startLoading(nextSequence);
    }));
}

void
ETLService::stop()
{
    LOG(log_.info()) << "Stop called";

    if (mainLoop_)
        mainLoop_->wait();
    if (taskMan_)
        taskMan_->stop();
    if (monitor_)
        monitor_->stop();
}

boost::json::object
ETLService::getInfo() const
{
    boost::json::object result;

    result["etl_sources"] = balancer_->toJson();
    result["is_writer"] = static_cast<int>(state_->isWriting);
    result["read_only"] = static_cast<int>(state_->isStrictReadonly);
    auto last = publisher_->getLastPublish();
    if (last.time_since_epoch().count() != 0)
        result["last_publish_age_seconds"] = std::to_string(publisher_->lastPublishAgeSeconds());
    return result;
}

bool
ETLService::isAmendmentBlocked() const
{
    return state_->isAmendmentBlocked;
}

bool
ETLService::isCorruptionDetected() const
{
    return state_->isCorruptionDetected;
}

std::optional<etl::ETLState>
ETLService::getETLState() const
{
    return balancer_->getETLState();
}

std::uint32_t
ETLService::lastCloseAgeSeconds() const
{
    return publisher_->lastCloseAgeSeconds();
}

std::optional<data::LedgerRange>
ETLService::loadInitialLedgerIfNeeded()
{
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (not rng.has_value()) {
        ASSERT(
            not state_->isStrictReadonly,
            "Database is empty but this node is in strict readonly mode. Can't write initial ledger."
        );

        LOG(log_.info()) << "Database is empty. Will download a ledger from the network.";
        state_->isWriting = true;  // immediately become writer as the db is empty

        auto const getMostRecent = [this]() {
            LOG(log_.info()) << "Waiting for next ledger to be validated by network...";
            return ledgers_->getMostRecent();
        };

        if (auto const maybeSeq = startSequence_.or_else(getMostRecent); maybeSeq.has_value()) {
            auto const seq = *maybeSeq;
            LOG(log_.info()) << "Starting from sequence " << seq
                             << ". Initial ledger download and extraction can take a while...";

            auto [ledger, timeDiff] = ::util::timed<std::chrono::duration<double>>([this, seq]() {
                return extractor_->extractLedgerOnly(seq).and_then(
                    [this, seq](auto&& data) -> std::optional<ripple::LedgerHeader> {
                        // TODO: loadInitialLedger in balancer should be called fetchEdgeKeys or similar
                        auto res = balancer_->loadInitialLedger(seq, *initialLoadObserver_);
                        if (not res.has_value() and res.error() == InitialLedgerLoadError::Cancelled) {
                            LOG(log_.debug()) << "Initial ledger load got cancelled";
                            return std::nullopt;
                        }

                        ASSERT(res.has_value(), "Initial ledger retry logic failed");
                        data.edgeKeys = std::move(res).value();

                        return loader_->loadInitialLedger(data);
                    }
                );
            });

            if (not ledger.has_value()) {
                LOG(log_.error()) << "Failed to load initial ledger. Exiting monitor loop";
                return std::nullopt;
            }

            LOG(log_.debug()) << "Time to download and store ledger = " << timeDiff;
            LOG(log_.info()) << "Finished loadInitialLedger. cache size = " << backend_->cache().size();

            return backend_->hardFetchLedgerRangeNoThrow();
        }

        LOG(log_.info()) << "The wait for the next validated ledger has been aborted. "
                            "Exiting monitor loop";
        return std::nullopt;
    }

    LOG(log_.info()) << "Database already populated. Picking up from the tip of history";
    cacheLoader_->load(rng->maxSequence);

    return rng;
}

void
ETLService::startMonitor(uint32_t seq)
{
    monitor_ = monitorProvider_->make(ctx_, backend_, ledgers_, seq);

    monitorNewSeqSubscription_ = monitor_->subscribeToNewSequence([this](uint32_t seq) {
        LOG(log_.info()) << "ETLService (via Monitor) got new seq from db: " << seq;

        if (state_->writeConflict) {
            LOG(log_.info()) << "Got a write conflict; Giving up writer seat immediately";
            giveUpWriter();
        }

        if (not state_->isWriting) {
            auto const diff = data::synchronousAndRetryOnTimeout([this, seq](auto yield) {
                return backend_->fetchLedgerDiff(seq, yield);
            });

            cacheUpdater_->update(seq, diff);
            backend_->updateRange(seq);
        }

        publisher_->publish(seq, {});
    });

    monitorDbStalledSubscription_ = monitor_->subscribeToDbStalled([this]() {
        LOG(log_.warn()) << "ETLService received DbStalled signal from Monitor";
        if (not state_->isStrictReadonly and not state_->isWriting)
            attemptTakeoverWriter();
    });

    monitor_->run();
}

void
ETLService::startLoading(uint32_t seq)
{
    ASSERT(not state_->isStrictReadonly, "This should only happen on writer nodes");
    taskMan_ = taskManagerProvider_->make(ctx_, *monitor_, seq, finishSequence_);
    taskMan_->run(config_.get().get<std::size_t>("extractor_threads"));
}

void
ETLService::attemptTakeoverWriter()
{
    ASSERT(not state_->isStrictReadonly, "This should only happen on writer nodes");
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    ASSERT(rng.has_value(), "Ledger range can't be null");

    state_->isWriting = true;  // switch to writer
    LOG(log_.info()) << "Taking over the ETL writer seat";
    startLoading(rng->maxSequence + 1);
}

void
ETLService::giveUpWriter()
{
    ASSERT(not state_->isStrictReadonly, "This should only happen on writer nodes");
    state_->isWriting = false;
    state_->writeConflict = false;
    taskMan_ = nullptr;
}

}  // namespace etlng
