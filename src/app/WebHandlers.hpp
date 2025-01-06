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

#include "rpc/Errors.hpp"
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

public:
    /**
     * @brief Construct a new MetricsHandler object
     *
     * @param adminVerifier The AdminVerificationStrategy to use for verifying the connection for admin access.
     */
    MetricsHandler(std::shared_ptr<web::AdminVerificationStrategy> adminVerifier);

    /**
     * @brief The call of the function object.
     *
     * @param request The request to handle.
     * @param connectionMetadata The connection metadata.
     * @return The response to the request.
     */
    web::ng::Response
    operator()(
        web::ng::Request const& request,
        web::ng::ConnectionMetadata& connectionMetadata,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
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
 * @brief A function object that handles the websocket endpoint.
 *
 * @tparam RpcHandlerType The type of the RPC handler.
 */
template <typename RpcHandlerType>
class RequestHandler {
    util::Logger webServerLog_{"WebServer"};
    std::shared_ptr<web::AdminVerificationStrategy> adminVerifier_;
    std::reference_wrapper<RpcHandlerType> rpcHandler_;
    std::reference_wrapper<web::dosguard::DOSGuardInterface> dosguard_;

public:
    /**
     * @brief Construct a new RequestHandler object
     *
     * @param adminVerifier The AdminVerificationStrategy to use for verifying the connection for admin access.
     * @param rpcHandler The RPC handler to use for handling the request.
     * @param dosguard The DOSGuardInterface to use for checking the connection.
     */
    RequestHandler(
        std::shared_ptr<web::AdminVerificationStrategy> adminVerifier,
        RpcHandlerType& rpcHandler,
        web::dosguard::DOSGuardInterface& dosguard
    )
        : adminVerifier_(std::move(adminVerifier)), rpcHandler_(rpcHandler), dosguard_(dosguard)
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
        if (not dosguard_.get().request(connectionMetadata.ip())) {
            auto error = rpc::makeError(rpc::RippledError::rpcSLOW_DOWN);

            if (not request.isHttp()) {
                try {
                    auto requestJson = boost::json::parse(request.message());
                    if (requestJson.is_object() && requestJson.as_object().contains("id"))
                        error["id"] = requestJson.as_object().at("id");
                    error["request"] = request.message();
                } catch (std::exception const&) {
                    error["request"] = request.message();
                }
            }
            return web::ng::Response{boost::beast::http::status::service_unavailable, error, request};
        }
        LOG(webServerLog_.info()) << connectionMetadata.tag()
                                  << "Received request from ip = " << connectionMetadata.ip()
                                  << " - posting to WorkQueue";

        connectionMetadata.setIsAdmin([this, &request, &connectionMetadata]() {
            return adminVerifier_->isAdmin(request.httpHeaders(), connectionMetadata.ip());
        });

        try {
            auto response = rpcHandler_(request, connectionMetadata, std::move(subscriptionContext), yield);

            if (not dosguard_.get().add(connectionMetadata.ip(), response.message().size())) {
                auto jsonResponse = boost::json::parse(response.message()).as_object();
                jsonResponse["warning"] = "load";
                if (jsonResponse.contains("warnings") && jsonResponse["warnings"].is_array()) {
                    jsonResponse["warnings"].as_array().push_back(rpc::makeWarning(rpc::WarnRpcRateLimit));
                } else {
                    jsonResponse["warnings"] = boost::json::array{rpc::makeWarning(rpc::WarnRpcRateLimit)};
                }
                response.setMessage(jsonResponse);
            }

            return response;
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
