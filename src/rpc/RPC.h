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
#include <config/Config.h>
#include <log/Logger.h>
#include <rpc/Counters.h>
#include <rpc/Errors.h>
#include <rpc/HandlerTable.h>
#include <rpc/common/AnyHandler.h>
#include <util/Taggable.h>
#include <webserver/DOSGuard.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

/*
 * This file contains various classes necessary for executing RPC handlers.
 * Context gives the handlers access to various other parts of the application
 * Status is used to report errors.
 * And lastly, there are various functions for making Contexts, Statuses and
 * serializing Status to JSON.
 * This file is meant to contain any class or function that code outside of the
 * rpc folder needs to use. For helper functions or classes used within the rpc
 * folder, use RPCHelpers.h.
 */

class WsBase;
class SubscriptionManager;
class ETLLoadBalancer;
class ReportingETL;

namespace RPC {

struct Context : public util::Taggable
{
    clio::Logger perfLog_{"Performance"};
    boost::asio::yield_context& yield;
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    std::shared_ptr<WsBase> session;
    Backend::LedgerRange const& range;
    std::string clientIp;

    Context(
        boost::asio::yield_context& yield_,
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        std::shared_ptr<WsBase> const& session_,
        util::TagDecoratorFactory const& tagFactory_,
        Backend::LedgerRange const& range_,
        std::string const& clientIp_);
};

struct AccountCursor
{
    ripple::uint256 index;
    std::uint32_t hint;

    std::string
    toString() const
    {
        return ripple::strHex(index) + "," + std::to_string(hint);
    }

    bool
    isNonZero() const
    {
        return index.isNonZero() || hint != 0;
    }
};

using Result = std::variant<Status, boost::json::object>;

std::optional<Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<WsBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    std::string const& clientIp);

std::optional<Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    std::string const& clientIp);

/**
 * @brief The RPC engine that ties all RPC-related functionality together
 */
class RPCEngine
{
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    std::reference_wrapper<clio::DOSGuard const> dosGuard_;

    HandlerTable handlerTable_;
    WorkQueue workQueue_;
    Counters counters_;

public:
    RPCEngine(
        std::shared_ptr<BackendInterface> const& backend,
        std::shared_ptr<SubscriptionManager> const& subscriptions,
        std::shared_ptr<ETLLoadBalancer> const& balancer,
        clio::DOSGuard const& dosGuard,
        uint32_t numWorkerThreads,
        uint32_t maxQueueSize)
        : backend_{backend}
        , subscriptions_{subscriptions}
        , balancer_{balancer}
        , dosGuard_{std::cref(dosGuard)}
        , handlerTable_{backend}
        , workQueue_{numWorkerThreads, maxQueueSize}
        , counters_{workQueue_}
    {
    }

    static std::shared_ptr<RPCEngine>
    make_RPCEngine(
        clio::Config const& config,
        std::shared_ptr<BackendInterface> const& backend,
        std::shared_ptr<SubscriptionManager> const& subscriptions,
        std::shared_ptr<ETLLoadBalancer> const& balancer,
        clio::DOSGuard const& dosGuard)
    {
        static clio::Logger log{"RPC"};
        auto const serverConfig = config.section("server");
        auto const numThreads = config.valueOr<uint32_t>(
            "workers", std::thread::hardware_concurrency());
        auto const maxQueueSize = serverConfig.valueOr<uint32_t>(
            "max_queue_size", 0);  // 0 is no limit

        log.info() << "Number of workers = " << numThreads
                   << ". Max queue size = " << maxQueueSize;

        return std::make_shared<RPCEngine>(
            backend,
            subscriptions,
            balancer,
            dosGuard,
            numThreads,
            maxQueueSize);
    }

    /**
     * @brief Main request processor routine
     * @param ctx The @ref Context of the request
     */
    Result
    buildResponse(Context const& ctx);

    /**
     * @brief Used to schedule request processing onto the work queue
     * @param func The lambda to execute when this request is handled
     * @param ip The ip address for which this request is being executed
     */
    template <typename Fn>
    bool
    post(Fn&& func, std::string const& ip)
    {
        return workQueue_.postCoro(
            std::forward<Fn>(func), dosGuard_.get().isWhiteListed(ip));
    }

    /**
     * @brief Notify the system that specified method was executed
     * @param method
     * @param duration The time it took to execute the method specified in
     * microseconds
     */
    void
    notifyComplete(
        std::string const& method,
        std::chrono::microseconds const& duration);

    void
    notifyErrored(std::string const& method);

private:
    bool
    shouldSuppressValidatedFlag(RPC::Context const& context) const;

    bool
    shouldForwardToRippled(Context const& ctx) const;

    bool
    isClioOnly(std::string const& method) const;

    bool
    validHandler(std::string const& method) const;
};

template <class T>
void
logDuration(Context const& ctx, T const& dur)
{
    static clio::Logger log{"RPC"};
    std::stringstream ss;
    ss << ctx.tag()
       << "Request processing duration = " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
       << " milliseconds. request = " << ctx.params;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    if (seconds > 10)
        log.error() << ss.str();
    else if (seconds > 1)
        log.warn() << ss.str();
    else
        log.info() << ss.str();
}

}  // namespace RPC
