//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "app/ClioApplication.hpp"

#include "app/Stopper.hpp"
#include "app/WebHandlers.hpp"
#include "data/AmendmentCenter.hpp"
#include "data/BackendFactory.hpp"
#include "etl/ETLService.hpp"
#include "etl/LoadBalancer.hpp"
#include "etl/NetworkValidatedLedgers.hpp"
#include "feed/SubscriptionManager.hpp"
#include "migration/MigrationInspectorFactory.hpp"
#include "rpc/Counters.hpp"
#include "rpc/RPCEngine.hpp"
#include "rpc/WorkQueue.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/build/Build.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/RPCServerHandler.hpp"
#include "web/Server.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/IntervalSweepHandler.hpp"
#include "web/dosguard/WhitelistHandler.hpp"
#include "web/ng/RPCServerHandler.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace app {

namespace {

/**
 * @brief Start context threads
 *
 * @param ioc Context
 * @param numThreads Number of worker threads to start
 */
void
start(boost::asio::io_context& ioc, std::uint32_t numThreads)
{
    std::vector<std::thread> v;
    v.reserve(numThreads - 1);
    for (auto i = numThreads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });

    ioc.run();
    for (auto& t : v)
        t.join();
}

}  // namespace

ClioApplication::ClioApplication(util::config::ClioConfigDefinition const& config)
    : config_(config), signalsHandler_{config_}
{
    LOG(util::LogService::info()) << "Clio version: " << util::build::getClioFullVersionString();
    PrometheusService::init(config);
    signalsHandler_.subscribeToStop([this]() { appStopper_.stop(); });
}

int
ClioApplication::run(bool const useNgWebServer)
{
    auto const threads = config_.get<uint16_t>("io_threads");
    LOG(util::LogService::info()) << "Number of io threads = " << threads;

    // IO context to handle all incoming requests, as well as other things.
    // This is not the only io context in the application.
    boost::asio::io_context ioc{threads};

    // Rate limiter, to prevent abuse
    auto whitelistHandler = web::dosguard::WhitelistHandler{config_};
    auto dosGuard = web::dosguard::DOSGuard{config_, whitelistHandler};
    auto sweepHandler = web::dosguard::IntervalSweepHandler{config_, ioc, dosGuard};

    // Interface to the database
    auto backend = data::makeBackend(config_);

    {
        auto const migrationInspector = migration::makeMigrationInspector(config_, backend);
        // Check if any migration is blocking Clio server starting.
        if (migrationInspector->isBlockingClio() and backend->hardFetchLedgerRangeNoThrow()) {
            LOG(util::LogService::error())
                << "Existing Migration is blocking Clio, Please complete the database migration first.";
            return EXIT_FAILURE;
        }
    }

    // Manages clients subscribed to streams
    auto subscriptions = feed::SubscriptionManager::makeSubscriptionManager(config_, backend);

    // Tracks which ledgers have been validated by the network
    auto ledgers = etl::NetworkValidatedLedgers::makeValidatedLedgers();

    // Handles the connection to one or more rippled nodes.
    // ETL uses the balancer to extract data.
    // The server uses the balancer to forward RPCs to a rippled node.
    // The balancer itself publishes to streams (transactions_proposed and accounts_proposed)
    auto balancer = etl::LoadBalancer::makeLoadBalancer(config_, ioc, backend, subscriptions, ledgers);

    // ETL is responsible for writing and publishing to streams. In read-only mode, ETL only publishes
    auto etl = etl::ETLService::makeETLService(config_, ioc, backend, subscriptions, balancer, ledgers);

    auto workQueue = rpc::WorkQueue::makeWorkQueue(config_);
    auto counters = rpc::Counters::makeCounters(workQueue);
    auto const amendmentCenter = std::make_shared<data::AmendmentCenter const>(backend);
    auto const handlerProvider = std::make_shared<rpc::impl::ProductionHandlerProvider const>(
        config_, backend, subscriptions, balancer, etl, amendmentCenter, counters
    );

    using RPCEngineType = rpc::RPCEngine<etl::LoadBalancer, rpc::Counters>;
    auto const rpcEngine =
        RPCEngineType::makeRPCEngine(config_, backend, balancer, dosGuard, workQueue, counters, handlerProvider);

    if (useNgWebServer or config_.get<bool>("server.__ng_web_server")) {
        web::ng::RPCServerHandler<RPCEngineType, etl::ETLService> handler{config_, backend, rpcEngine, etl};

        auto expectedAdminVerifier = web::makeAdminVerificationStrategy(config_);
        if (not expectedAdminVerifier.has_value()) {
            LOG(util::LogService::error()) << "Error creating admin verifier: " << expectedAdminVerifier.error();
            return EXIT_FAILURE;
        }
        auto const adminVerifier = std::move(expectedAdminVerifier).value();

        auto httpServer = web::ng::makeServer(config_, OnConnectCheck{dosGuard}, DisconnectHook{dosGuard}, ioc);

        if (not httpServer.has_value()) {
            LOG(util::LogService::error()) << "Error creating web server: " << httpServer.error();
            return EXIT_FAILURE;
        }

        httpServer->onGet("/metrics", MetricsHandler{adminVerifier});
        httpServer->onGet("/health", HealthCheckHandler{});
        auto requestHandler = RequestHandler{adminVerifier, handler, dosGuard};
        httpServer->onPost("/", requestHandler);
        httpServer->onWs(std::move(requestHandler));

        auto const maybeError = httpServer->run();
        if (maybeError.has_value()) {
            LOG(util::LogService::error()) << "Error starting web server: " << *maybeError;
            return EXIT_FAILURE;
        }

        appStopper_.setOnStop(
            Stopper::makeOnStopCallback(httpServer.value(), *balancer, *etl, *subscriptions, *backend, ioc)
        );

        // Blocks until stopped.
        // When stopped, shared_ptrs fall out of scope
        // Calls destructors on all resources, and destructs in order
        start(ioc, threads);

        return EXIT_SUCCESS;
    }

    // Init the web server
    auto handler =
        std::make_shared<web::RPCServerHandler<RPCEngineType, etl::ETLService>>(config_, backend, rpcEngine, etl);

    auto const httpServer = web::makeHttpServer(config_, ioc, dosGuard, handler);

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);

    return EXIT_SUCCESS;
}

}  // namespace app
