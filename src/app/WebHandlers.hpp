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

#include "data/LedgerCacheInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/WorkQueue.hpp"
#include "util/log/Logger.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/json/array.hpp>
#include <boost/json/parse.hpp>

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace app {

/**
 * @brief A function object that checks if the connection is allowed to proceed.
 */
class OnConnectCheck {
    std::reference_wrapper<web::dosguard::DOSGuardInterface> dosguard_;

public:
    /**
     * @brief Construct a new OnConnectCheck object
     *
     * @param dosguard The DOSGuardInterface to use for checking the connection.
     */
    OnConnectCheck(web::dosguard::DOSGuardInterface& dosguard);

    /**
     * @brief Check if the connection is allowed to proceed.
     *
     * @param connection The connection to check.
     * @return A response if the connection is not allowed to proceed or void otherwise.
     */
    std::expected<void, web::ng::Response>
    operator()(web::ng::Connection const& connection);
};

/**
 * @brief A function object that is called when the IP of a connection changes (usually if proxy detected).
 * This is used to update the DOS guard.
 */
class IpChangeHook {
    std::reference_wrapper<web::dosguard::DOSGuardInterface> dosguard_;

public:
    /**
     * @brief Construct a new IpChangeHook object.
     *
     * @param dosguard The DOS guard to use.
     */
    IpChangeHook(web::dosguard::DOSGuardInterface& dosguard);

    /**
     * @brief The call of the function object.
     *
     * @param oldIp The old IP of the connection.
     * @param newIp The new IP of the connection.
     */
    void
    operator()(std::string const& oldIp, std::string const& newIp);
};

/**
 * @brief A function object to be called when a connection is disconnected.
 */
class DisconnectHook {
    std::reference_wrapper<web::dosguard::DOSGuardInterface> dosguard_;

public:
    /**
     * @brief Construct a new DisconnectHook object
     *
     * @param dosguard The DOSGuardInterface to use for disconnecting the connection.
     */
    DisconnectHook(web::dosguard::DOSGuardInterface& dosguard);

    /**
     * @brief The call of the function object.
     *
     * @param connection The connection which has disconnected.
     */
    void
    operator()(web::ng::Connection const& connection);
};

/**
 * @brief A function object that handles the metrics endpoint.
 */
class MetricsHandler {
    std::shared_ptr<web::AdminVerificationStrategy> adminVerifier_;
    std::reference_wrapper<rpc::WorkQueue> workQueue_;

public:
    /**
     * @brief Construct a new MetricsHandler object
     *
     * @param adminVerifier The AdminVerificationStrategy to use for verifying the connection for admin access.
     * @param workQueue The WorkQueue to use for handling the request.
     */
    MetricsHandler(std::shared_ptr<web::AdminVerificationStrategy> adminVerifier, rpc::WorkQueue& workQueue);

    /**
     * @brief The call of the function object.
     *
     * @param request The request to handle.
     * @param connectionMetadata The connection metadata.
     * @param yield The yield context.
     * @return The response to the request.
     */
    web::ng::Response
    operator()(
        web::ng::Request const& request,
        web::ng::ConnectionMetadata& connectionMetadata,
        web::SubscriptionContextPtr,
        boost::asio::yield_context yield
    );
};

/**
 * @brief A function object that handles the health check endpoint.
 */
class HealthCheckHandler {
public:
    /**
     * @brief The call of the function object.
     *
     * @param request The request to handle.
     * @return The response to the request
     */
    web::ng::Response
    operator()(
        web::ng::Request const& request,
        web::ng::ConnectionMetadata&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    );
};

/**
 * @brief A function object that handles the cache state check endpoint.
 */
class CacheStateHandler {
    std::reference_wrapper<data::LedgerCacheInterface const> cache_;

public:
    /**
     * @brief Construct a new CacheStateHandler object.
     *
     * @param cache The ledger cache to use.
     */
    CacheStateHandler(data::LedgerCacheInterface const& cache) : cache_{cache}
    {
    }

    /**
     * @brief The call of the function object.
     *
     * @param request The request to handle.
     * @return The response to the request
     */
    web::ng::Response
    operator()(
        web::ng::Request const& request,
        web::ng::ConnectionMetadata&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    );
};

/**
 * @brief A function object that handles the websocket endpoint.
 *
 * @tparam RpcHandlerType The type of the RPC handler.
 */
template <typename RpcHandlerType>
class RequestHandler {
    util::Logger webServerLog_{"WebServer"};
    std::shared_ptr<web::AdminVerificationStrategy> adminVerifier_;
    std::reference_wrapper<RpcHandlerType> rpcHandler_;

public:
    /**
     * @brief Construct a new RequestHandler object
     *
     * @param adminVerifier The AdminVerificationStrategy to use for verifying the connection for admin access.
     * @param rpcHandler The RPC handler to use for handling the request.
     */
    RequestHandler(std::shared_ptr<web::AdminVerificationStrategy> adminVerifier, RpcHandlerType& rpcHandler)
        : adminVerifier_(std::move(adminVerifier)), rpcHandler_(rpcHandler)
    {
    }

    /**
     * @brief The call of the function object.
     *
     * @param request The request to handle.
     * @param connectionMetadata The connection metadata.
     * @param subscriptionContext The subscription context.
     * @param yield The yield context.
     * @return The response to the request.
     */
    web::ng::Response
    operator()(
        web::ng::Request const& request,
        web::ng::ConnectionMetadata& connectionMetadata,
        web::SubscriptionContextPtr subscriptionContext,
        boost::asio::yield_context yield
    )
    {
        LOG(webServerLog_.info()) << connectionMetadata.tag()
                                  << "Received request from ip = " << connectionMetadata.ip()
                                  << " - posting to WorkQueue";

        connectionMetadata.setIsAdmin([this, &request, &connectionMetadata]() {
            return adminVerifier_->isAdmin(request.httpHeaders(), connectionMetadata.ip());
        });

        try {
            return rpcHandler_(request, connectionMetadata, std::move(subscriptionContext), yield);
        } catch (std::exception const&) {
            return web::ng::Response{
                boost::beast::http::status::internal_server_error,
                rpc::makeError(rpc::RippledError::rpcINTERNAL),
                request
            };
        }
    }
};

}  // namespace app
