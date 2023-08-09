//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <rpc/Errors.h>
#include <rpc/Factories.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/impl/APIVersionParser.h>
#include <util/JsonUtils.h>
#include <util/Profiler.h>
#include <webserver/impl/ErrorHandling.h>

#include <boost/json/parse.hpp>

/**
 * @brief The server handler for RPC requests called by web server
 *
 * Note: see ServerHandler concept
 */
template <class Engine, class ETL>
class RPCServerHandler
{
    std::shared_ptr<BackendInterface const> const backend_;
    std::shared_ptr<Engine> const rpcEngine_;
    std::shared_ptr<ETL const> const etl_;
    // subscription manager holds the shared_ptr of this class
    std::weak_ptr<SubscriptionManager> const subscriptions_;
    clio::util::TagDecoratorFactory const tagFactory_;
    RPC::detail::ProductionAPIVersionParser apiVersionParser_;  // can be injected if needed

    clio::util::Logger log_{"RPC"};
    clio::util::Logger perfLog_{"Performance"};

public:
    RPCServerHandler(
        clio::util::Config const& config,
        std::shared_ptr<BackendInterface const> const& backend,
        std::shared_ptr<Engine> const& rpcEngine,
        std::shared_ptr<ETL const> const& etl,
        std::shared_ptr<SubscriptionManager> const& subscriptions)
        : backend_(backend)
        , rpcEngine_(rpcEngine)
        , etl_(etl)
        , subscriptions_(subscriptions)
        , tagFactory_(config)
        , apiVersionParser_(config.sectionOr("api_version", {}))
    {
    }

    /**
     * @brief The callback when server receives a request
     * @param req The request
     * @param connection The connection
     */
    void
    operator()(std::string const& reqStr, std::shared_ptr<web::ConnectionBase> const& connection)
    {
        try
        {
            auto req = boost::json::parse(reqStr).as_object();
            perfLog_.debug() << connection->tag() << "Adding to work queue";

            if (not connection->upgraded and not req.contains("params"))
                req["params"] = boost::json::array({boost::json::object{}});

            if (!rpcEngine_->post(
                    [this, request = std::move(req), connection](boost::asio::yield_context yield) mutable {
                        handleRequest(yield, std::move(request), connection);
                    },
                    connection->clientIp))
            {
                rpcEngine_->notifyTooBusy();
                web::detail::ErrorHelper(connection).sendTooBusyError();
            }
        }
        catch (boost::system::system_error const& ex)
        {
            // system_error thrown when json parsing failed
            rpcEngine_->notifyBadSyntax();
            web::detail::ErrorHelper(connection).sendJsonParsingError(ex.what());
        }
        catch (std::invalid_argument const& ex)
        {
            // thrown when json parses something that is not an object at top level
            rpcEngine_->notifyBadSyntax();
            web::detail::ErrorHelper(connection).sendJsonParsingError(ex.what());
        }
        catch (std::exception const& ex)
        {
            perfLog_.error() << connection->tag() << "Caught exception: " << ex.what();
            rpcEngine_->notifyInternalError();
            throw;
        }
    }

    /**
     * @brief The callback when there is an error.
     * Remove the session shared ptr from subscription manager
     * @param _ The error code
     * @param connection The connection
     */
    void
    operator()(boost::beast::error_code _, std::shared_ptr<web::ConnectionBase> const& connection)
    {
        if (auto manager = subscriptions_.lock(); manager)
            manager->cleanup(connection);
    }

private:
    void
    handleRequest(
        boost::asio::yield_context yield,
        boost::json::object&& request,
        std::shared_ptr<web::ConnectionBase> const& connection)
    {
        log_.info() << connection->tag() << (connection->upgraded ? "ws" : "http")
                    << " received request from work queue: " << clio::util::removeSecret(request)
                    << " ip = " << connection->clientIp;

        try
        {
            auto const range = backend_->fetchLedgerRange();
            if (!range)
            {
                // for error that happened before the handler, we don't attach any warnings
                rpcEngine_->notifyNotReady();
                return web::detail::ErrorHelper(connection, request).sendNotReadyError();
            }

            auto const context = [&] {
                if (connection->upgraded)
                    return RPC::make_WsContext(
                        yield,
                        request,
                        connection,
                        tagFactory_.with(connection->tag()),
                        *range,
                        connection->clientIp,
                        std::cref(apiVersionParser_));
                else
                    return RPC::make_HttpContext(
                        yield,
                        request,
                        tagFactory_.with(connection->tag()),
                        *range,
                        connection->clientIp,
                        std::cref(apiVersionParser_));
            }();

            if (!context)
            {
                auto const err = context.error();
                perfLog_.warn() << connection->tag() << "Could not create Web context: " << err;
                log_.warn() << connection->tag() << "Could not create Web context: " << err;

                // we count all those as BadSyntax - as the WS path would.
                // Although over HTTP these will yield a 400 status with a plain text response (for most).
                rpcEngine_->notifyBadSyntax();
                return web::detail::ErrorHelper(connection, request).sendError(err);
            }

            auto [v, timeDiff] = clio::util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            RPC::logDuration(*context, us);

            boost::json::object response;
            if (auto const status = std::get_if<RPC::Status>(&v))
            {
                // note: error statuses are counted/notified in buildResponse itself
                response = web::detail::ErrorHelper(connection, request).composeError(*status);
                auto const responseStr = boost::json::serialize(response);

                perfLog_.debug() << context->tag() << "Encountered error: " << responseStr;
                log_.debug() << context->tag() << "Encountered error: " << responseStr;
            }
            else
            {
                // This can still technically be an error. Clio counts forwarded requests as successful.
                rpcEngine_->notifyComplete(context->method, us);

                auto& result = std::get<boost::json::object>(v);
                auto const isForwarded = result.contains("forwarded") && result.at("forwarded").is_bool() &&
                    result.at("forwarded").as_bool();

                // if the result is forwarded - just use it as is
                // if forwarded request has error, for http, error should be in "result"; for ws, error should be at top
                if (isForwarded && (result.contains("result") || connection->upgraded))
                {
                    for (auto const& [k, v] : result)
                        response.insert_or_assign(k, v);
                }
                else
                {
                    response["result"] = result;
                }

                // for ws there is an additional field "status" in the response,
                // otherwise the "status" is in the "result" field
                if (connection->upgraded)
                {
                    auto const id = request.contains("id") ? request.at("id") : nullptr;

                    if (not id.is_null())
                        response["id"] = id;

                    if (!response.contains("error"))
                        response["status"] = "success";

                    response["type"] = "response";
                }
                else
                {
                    if (response.contains("result") && !response["result"].as_object().contains("error"))
                        response["result"].as_object()["status"] = "success";
                }
            }

            boost::json::array warnings;
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_CLIO));

            if (etl_->lastCloseAgeSeconds() >= 60)
                warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_OUTDATED));

            response["warnings"] = warnings;
            connection->send(boost::json::serialize(response));
        }
        catch (std::exception const& ex)
        {
            // note: while we are catching this in buildResponse too, this is here to make sure
            // that any other code that may throw is outside of buildResponse is also worked around.
            perfLog_.error() << connection->tag() << "Caught exception: " << ex.what();
            log_.error() << connection->tag() << "Caught exception: " << ex.what();

            rpcEngine_->notifyInternalError();
            return web::detail::ErrorHelper(connection, request).sendInternalError();
        }
    }
};
