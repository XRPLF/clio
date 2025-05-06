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

#pragma once

#include "data/BackendInterface.hpp"
#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "etl/CacheLoader.hpp"
#include "etl/ETLState.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etl/impl/LedgerPublisher.hpp"
#include "etlng/AmendmentBlockHandlerInterface.hpp"
#include "etlng/ETLServiceInterface.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/LoadBalancerInterface.hpp"
#include "etlng/impl/AmendmentBlockHandler.hpp"
#include "etlng/impl/Extraction.hpp"
#include "etlng/impl/Loading.hpp"
#include "etlng/impl/Monitor.hpp"
#include "etlng/impl/Registry.hpp"
#include "etlng/impl/Scheduling.hpp"
#include "etlng/impl/TaskManager.hpp"
#include "etlng/impl/ext/Cache.hpp"
#include "etlng/impl/ext/Core.hpp"
#include "etlng/impl/ext/NFT.hpp"
#include "etlng/impl/ext/Successor.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/Assert.hpp"
#include "util/Profiler.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/json/object.hpp>
#include <fmt/core.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace etlng {

/**
 * @brief This class is responsible for continuously extracting data from a p2p node, and writing that data to the
 * databases.
 *
 * Usually, multiple different processes share access to the same network accessible databases, in which case only one
 * such process is performing ETL and writing to the database. The other processes simply monitor the database for new
 * ledgers, and publish those ledgers to the various subscription streams. If a monitoring process determines that the
 * ETL writer has failed (no new ledgers written for some time), the process will attempt to become the ETL writer.
 *
 * If there are multiple monitoring processes that try to become the ETL writer at the same time, one will win out, and
 * the others will fall back to monitoring/publishing. In this sense, this class dynamically transitions from monitoring
 * to writing and from writing to monitoring, based on the activity of other processes running on different machines.
 */
class ETLService : public ETLServiceInterface {
    util::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;
    std::shared_ptr<etlng::LoadBalancerInterface> balancer_;
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> ledgers_;
    std::shared_ptr<etl::CacheLoader<>> cacheLoader_;

    std::shared_ptr<etl::LedgerFetcherInterface> fetcher_;
    std::shared_ptr<ExtractorInterface> extractor_;

    etl::SystemState state_;
    util::async::CoroExecutionContext ctx_{8};

    std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler_;
    std::shared_ptr<impl::Loader> loader_;

    std::optional<util::async::CoroExecutionContext::Operation<void>> mainLoop_;

public:
    /**
     * @brief Create an instance of ETLService.
     *
     * @param config The configuration to use
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param balancer Load balancer to use
     * @param ledgers The network validated ledgers datastructure
     */
    ETLService(
        util::config::ClioConfigDefinition const& config,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        std::shared_ptr<etlng::LoadBalancerInterface> balancer,
        std::shared_ptr<etl::NetworkValidatedLedgersInterface> ledgers
    )
        : backend_(std::move(backend))
        , subscriptions_(std::move(subscriptions))
        , balancer_(std::move(balancer))
        , ledgers_(std::move(ledgers))
        , cacheLoader_(std::make_shared<etl::CacheLoader<>>(config, backend_, backend_->cache()))
        , fetcher_(std::make_shared<etl::impl::LedgerFetcher>(backend_, balancer_))
        , extractor_(std::make_shared<impl::Extractor>(fetcher_))
        , amendmentBlockHandler_(std::make_shared<etlng::impl::AmendmentBlockHandler>(ctx_, state_))
        , loader_(std::make_shared<impl::Loader>(
              backend_,
              fetcher_,
              impl::makeRegistry(
                  impl::CacheExt{backend_->cache()},
                  impl::CoreExt{backend_},
                  impl::SuccessorExt{backend_, backend_->cache()},
                  impl::NFTExt{backend_}
              ),
              amendmentBlockHandler_
          ))
    {
        LOG(log_.info()) << "Creating ETLng...";
    }

    ~ETLService() override
    {
        LOG(log_.debug()) << "Stopping ETLng";
    }

    void
    run() override
    {
        LOG(log_.info()) << "run() in ETLng...";

        mainLoop_.emplace(ctx_.execute([this] {
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

            auto scheduler = impl::makeScheduler(impl::ForwardScheduler{*ledgers_, nextSequence}
                                                 // impl::BackfillScheduler{nextSequence - 1, nextSequence - 1000},
                                                 // TODO lift limit and start with rng.minSeq
            );

            auto man = impl::TaskManager(ctx_, *scheduler, *extractor_, *loader_);

            // TODO: figure out this: std::make_shared<impl::Monitor>(backend_, ledgers_, nextSequence)
            man.run({});  // TODO: needs to be interruptible and fill out settings
        }));
    }

    void
    stop() override
    {
        LOG(log_.info()) << "Stop called";
        // TODO: stop the service correctly
    }

    boost::json::object
    getInfo() const override
    {
        // TODO
        return {{"ok", true}};
    }

    bool
    isAmendmentBlocked() const override
    {
        // TODO
        return false;
    }

    bool
    isCorruptionDetected() const override
    {
        // TODO
        return false;
    }

    std::optional<etl::ETLState>
    getETLState() const override
    {
        // TODO
        return std::nullopt;
    }

    std::uint32_t
    lastCloseAgeSeconds() const override
    {
        // TODO
        return 0;
    }

private:
    // TODO: this better be std::expected
    std::optional<data::LedgerRange>
    loadInitialLedgerIfNeeded()
    {
        if (auto rng = backend_->hardFetchLedgerRangeNoThrow(); not rng.has_value()) {
            LOG(log_.info()) << "Database is empty. Will download a ledger from the network.";

            try {
                LOG(log_.info()) << "Waiting for next ledger to be validated by network...";
                if (auto const mostRecentValidated = ledgers_->getMostRecent(); mostRecentValidated.has_value()) {
                    auto const seq = *mostRecentValidated;
                    LOG(log_.info()) << "Ledger " << seq << " has been validated. Downloading... ";

                    auto [ledger, timeDiff] = ::util::timed<std::chrono::duration<double>>([this, seq]() {
                        return extractor_->extractLedgerOnly(seq).and_then([this, seq](auto&& data) {
                            // TODO: loadInitialLedger in balancer should be called fetchEdgeKeys or similar
                            data.edgeKeys = balancer_->loadInitialLedger(seq, *loader_);

                            // TODO: this should be interruptible for graceful shutdown
                            return loader_->loadInitialLedger(data);
                        });
                    });

                    LOG(log_.debug()) << "Time to download and store ledger = " << timeDiff;
                    LOG(log_.info()) << "Finished loadInitialLedger. cache size = " << backend_->cache().size();

                    if (ledger.has_value())
                        return backend_->hardFetchLedgerRangeNoThrow();

                    LOG(log_.error()) << "Failed to load initial ledger. Exiting monitor loop";
                } else {
                    LOG(log_.info()) << "The wait for the next validated ledger has been aborted. "
                                        "Exiting monitor loop";
                }
            } catch (std::runtime_error const& e) {
                LOG(log_.fatal()) << "Failed to load initial ledger: " << e.what();
                amendmentBlockHandler_->notifyAmendmentBlocked();
            }
        } else {
            LOG(log_.info()) << "Database already populated. Picking up from the tip of history";
            cacheLoader_->load(rng->maxSequence);

            return rng;
        }

        return std::nullopt;
    }
};
}  // namespace etlng
