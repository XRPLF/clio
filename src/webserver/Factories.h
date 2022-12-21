//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <backend/BackendInterface.h>
#include <etl/ETLHelpers.h>
#include <etl/ETLSource.h>
#include <subscriptions/SubscriptionManager.h>
#include <util/config/Config.h>
#include <webserver/Listener.h>

#include <memory>

namespace clio::web {

using namespace data;
using namespace etl;
using namespace subscription;
using namespace util;

static std::shared_ptr<HttpServer>
make_HttpServer(
    Config const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<ssl::context>> sslCtx,
    std::shared_ptr<BackendInterface const> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<ReportingETL const> etl,
    DOSGuard& dosGuard)
{
    static Logger log{"WebServer"};
    if (!config.contains("server"))
        return nullptr;

    auto const serverConfig = config.section("server");
    auto const address =
        boost::asio::ip::make_address(serverConfig.value<std::string>("ip"));
    auto const port = serverConfig.value<unsigned short>("port");
    auto const numThreads = config.valueOr<uint32_t>(
        "workers", std::thread::hardware_concurrency());
    auto const maxQueueSize =
        serverConfig.valueOr<uint32_t>("max_queue_size", 0);  // 0 is no limit

    log.info() << "Number of workers = " << numThreads
               << ". Max queue size = " << maxQueueSize;

    auto server = std::make_shared<HttpServer>(
        ioc,
        numThreads,
        maxQueueSize,
        sslCtx,
        boost::asio::ip::tcp::endpoint{address, port},
        backend,
        subscriptions,
        balancer,
        etl,
        util::TagDecoratorFactory(config),
        dosGuard);

    server->run();
    return server;
}

}  // namespace clio::web
