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

#pragma once

#include "data/BackendInterface.hpp"
#include "etl/ETLService.hpp"
#include "etl/LoadBalancer.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/CoroutineGroup.hpp"
#include "util/log/Logger.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <functional>
#include <thread>

namespace app {

/**
 * @brief Application stopper class. On stop it will create a new thread to run all the shutdown tasks.
 */
class Stopper {
    boost::asio::io_context ctx_;
    std::thread worker_;

public:
    /**
     * @brief Destroy the Stopper object
     */
    ~Stopper();

    /**
     * @brief Set the callback to be called when the application is stopped.
     *
     * @param cb The callback to be called on application stop.
     */
    void
    setOnStop(std::function<void(boost::asio::yield_context)> cb);

    /**
     * @brief Stop the application and run the shutdown tasks.
     */
    void
    stop();

    /**
     * @brief Create a callback to be called on application stop.
     *
     * @param server The server to stop.
     * @param balancer The load balancer to stop.
     * @param etl The ETL service to stop.
     * @param subscriptions The subscription manager to stop.
     * @param backend The backend to stop.
     * @param ioc The io_context to stop.
     * @return The callback to be called on application stop.
     */
    template <
        web::ng::SomeServer ServerType,
        etl::SomeLoadBalancer LoadBalancerType,
        etl::SomeETLService ETLServiceType>
    static std::function<void(boost::asio::yield_context)>
    makeOnStopCallback(
        ServerType& server,
        LoadBalancerType& balancer,
        ETLServiceType& etl,
        feed::SubscriptionManagerInterface& subscriptions,
        data::BackendInterface& backend,
        boost::asio::io_context& ioc
    )
    {
        return [&](boost::asio::yield_context yield) {
            util::CoroutineGroup coroutineGroup{yield};
            coroutineGroup.spawn(yield, [&server](auto innerYield) {
                server.stop(innerYield);
                LOG(util::LogService::info()) << "Server stopped";
            });
            coroutineGroup.spawn(yield, [&balancer](auto innerYield) {
                balancer.stop(innerYield);
                LOG(util::LogService::info()) << "LoadBalancer stopped";
            });
            coroutineGroup.asyncWait(yield);

            etl.stop();
            LOG(util::LogService::info()) << "ETL stopped";

            subscriptions.stop();
            LOG(util::LogService::info()) << "SubscriptionManager stopped";

            backend.waitForWritesToFinish();
            LOG(util::LogService::info()) << "Backend writes finished";

            ioc.stop();
            LOG(util::LogService::info()) << "io_context stopped";
        };
    }
};

}  // namespace app
