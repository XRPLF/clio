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
#include "data/Types.hpp"
#include "etl/ETLState.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etlng/CacheLoaderInterface.hpp"
#include "etlng/CacheUpdaterInterface.hpp"
#include "etlng/ETLServiceInterface.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/LedgerPublisherInterface.hpp"
#include "etlng/LoadBalancerInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/MonitorInterface.hpp"
#include "etlng/MonitorProviderInterface.hpp"
#include "etlng/TaskManagerInterface.hpp"
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
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/object.hpp>
#include <boost/signals2/connection.hpp>
#include <fmt/format.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

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

    util::async::AnyExecutionContext ctx_;
    std::reference_wrapper<util::config::ClioConfigDefinition const> config_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerInterface> balancer_;
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> ledgers_;
    std::shared_ptr<LedgerPublisherInterface> publisher_;
    std::shared_ptr<CacheLoaderInterface> cacheLoader_;
    std::shared_ptr<CacheUpdaterInterface> cacheUpdater_;
    std::shared_ptr<ExtractorInterface> extractor_;
    std::shared_ptr<LoaderInterface> loader_;
    std::shared_ptr<InitialLoadObserverInterface> initialLoadObserver_;
    std::shared_ptr<etlng::TaskManagerProviderInterface> taskManagerProvider_;
    std::shared_ptr<etlng::MonitorProviderInterface> monitorProvider_;
    std::shared_ptr<etl::SystemState> state_;

    std::optional<uint32_t> startSequence_;
    std::optional<uint32_t> finishSequence_;

    std::unique_ptr<MonitorInterface> monitor_;
    std::unique_ptr<TaskManagerInterface> taskMan_;

    boost::signals2::scoped_connection monitorNewSeqSubscription_;
    boost::signals2::scoped_connection monitorDbStalledSubscription_;

    std::optional<util::async::AnyOperation<void>> mainLoop_;

public:
    /**
     * @brief Create an instance of ETLService.
     *
     * @param ctx The execution context for asynchronous operations
     * @param config The Clio configuration definition
     * @param backend Interface to the backend database
     * @param balancer Load balancer for distributing work
     * @param ledgers Interface for accessing network validated ledgers
     * @param publisher Interface for publishing ledger data
     * @param cacheLoader Interface for loading cache data
     * @param cacheUpdater Interface for updating cache data
     * @param extractor The extractor to use
     * @param loader Interface for loading data
     * @param initialLoadObserver The observer for initial data loading
     * @param taskManagerProvider The provider of the task manager instance
     * @param monitorProvider The provider of the monitor instance
     * @param state System state tracking object
     */
    ETLService(
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
    );

    ~ETLService() override;

    void
    run() override;

    void
    stop() override;

    boost::json::object
    getInfo() const override;

    bool
    isAmendmentBlocked() const override;

    bool
    isCorruptionDetected() const override;

    std::optional<etl::ETLState>
    getETLState() const override;

    std::uint32_t
    lastCloseAgeSeconds() const override;

private:
    std::optional<data::LedgerRange>
    loadInitialLedgerIfNeeded();

    void
    startMonitor(uint32_t seq);

    void
    startLoading(uint32_t seq);

    void
    attemptTakeoverWriter();

    void
    giveUpWriter();
};

}  // namespace etlng
