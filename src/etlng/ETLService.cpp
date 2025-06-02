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
#include "etlng/TaskManagerProviderInterface.hpp"
#include "etlng/impl/AmendmentBlockHandler.hpp"
#include "etlng/impl/CacheUpdater.hpp"
#include "etlng/impl/Extraction.hpp"
#include "etlng/impl/LedgerPublisher.hpp"
#include "etlng/impl/Loading.hpp"
#include "etlng/impl/Monitor.hpp"
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
#include <fmt/core.h>

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
    , state_(std::move(state))
{
    LOG(log_.info()) << "Creating ETLng...";
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

    // TODO: write-enabled node should start in readonly and do the 10 second dance to become a writer
    mainLoop_.emplace(ctx_.execute([this] {
        state_->isWriting =
            not state_->isReadOnly;  // TODO: this is now needed because we don't have a mechanism for readonly or
                                     // ETL writer node. remove later in favor of real mechanism

        auto const rng = loadInitialLedgerIfNeeded();

        LOG(log_.info()) << "Waiting for next ledger to be validated by network...";
        std::optional<uint32_t> const mostRecentValidated = ledgers_->getMostRecent();

        if (not mostRecentValidated) {
            LOG(log_.info()) << "The wait for the next validated ledger has been aborted. "
                                "Exiting monitor loop";
            return;
        }

        ASSERT(rng.has_value(), "Ledger range can't be null");
        auto const nextSequence = rng->maxSequence + 1;

        LOG(log_.debug()) << "Database is populated. Starting monitor loop. sequence = " << nextSequence;
        startMonitor(nextSequence);

        // TODO: we only want to run the full ETL task man if we are POSSIBLY a write node
        // but definitely not in strict readonly
        if (not state_->isReadOnly)
            startLoading(nextSequence);
    }));
}

void
ETLService::stop()
{
    LOG(log_.info()) << "Stop called";

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
    result["read_only"] = static_cast<int>(state_->isReadOnly);
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
        LOG(log_.info()) << "Database is empty. Will download a ledger from the network.";

        LOG(log_.info()) << "Waiting for next ledger to be validated by network...";
        if (auto const mostRecentValidated = ledgers_->getMostRecent(); mostRecentValidated.has_value()) {
            auto const seq = *mostRecentValidated;
            LOG(log_.info()) << "Ledger " << seq << " has been validated. Downloading... ";

            auto [ledger, timeDiff] = ::util::timed<std::chrono::duration<double>>([this, seq]() {
                return extractor_->extractLedgerOnly(seq).and_then([this, seq](auto&& data) {
                    // TODO: loadInitialLedger in balancer should be called fetchEdgeKeys or similar
                    data.edgeKeys = balancer_->loadInitialLedger(seq, *initialLoadObserver_);

                    // TODO: this should be interruptible for graceful shutdown
                    return loader_->loadInitialLedger(data);
                });
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
    monitor_ = std::make_unique<impl::Monitor>(ctx_, backend_, ledgers_, seq);
    monitorSubscription_ = monitor_->subscribe([this](uint32_t seq) {
        log_.info() << "MONITOR got new seq from db: " << seq;

        // FIXME: is this the best way?
        if (not state_->isWriting) {
            auto const diff = data::synchronousAndRetryOnTimeout([this, seq](auto yield) {
                return backend_->fetchLedgerDiff(seq, yield);
            });
            cacheUpdater_->update(seq, diff);
        }

        publisher_->publish(seq, {});
    });
    monitor_->run();
}

void
ETLService::startLoading(uint32_t seq)
{
    taskMan_ = taskManagerProvider_->make(ctx_, *monitor_, seq);
    taskMan_->run(config_.get().get<std::size_t>("extractor_threads"));
}

}  // namespace etlng
