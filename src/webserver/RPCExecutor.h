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

#include <rpc/Factories.h>
#include <rpc/RPCHelpers.h>
#include <util/Profiler.h>

#include <boost/json/parse.hpp>

/**
 * @brief The executor for RPC requests called by web server
 */
template <class Engine, class ETL>
class RPCExecutor
{
    std::shared_ptr<BackendInterface const> const backend_;
    std::shared_ptr<Engine> const rpcEngine_;
    std::shared_ptr<ETL const> const etl_;
    // subscription manager holds the shared_ptr of this class
    std::weak_ptr<SubscriptionManager> const subscriptions_;
    util::TagDecoratorFactory const tagFactory_;

    clio::Logger log_{"RPC"};
    clio::Logger perfLog_{"Performance"};

public:
    RPCExecutor(
        clio::Config const& config,
        std::shared_ptr<BackendInterface const> const& backend,
        std::shared_ptr<Engine> const& rpcEngine,
        std::shared_ptr<ETL const> const& etl,
        std::shared_ptr<SubscriptionManager> const& subscriptions)
        : backend_(backend), rpcEngine_(rpcEngine), etl_(etl), subscriptions_(subscriptions), tagFactory_(config)
    {
    }

    /**
     * @brief The callback when server receives a request
     * @param req The request
     * @param connection The connection
     */
    void
    operator()(std::string const& reqStr, std::shared_ptr<Server::ConnectionBase> const& connection)
    {
        try
        {
            auto req = boost::json::parse(reqStr).as_object();
            perfLog_.debug() << connection->tag() << "Adding to work queue";

            if (not connection->upgraded and not req.contains("params"))
                req["params"] = boost::json::array({boost::json::object{}});

            if (!rpcEngine_->post(
                    [request = std::move(req), connection, this](boost::asio::yield_context yc) mutable {
                        handleRequest(yc, std::move(request), connection);
                    },
                    connection->clientIp))
            {
                rpcEngine_->notifyTooBusy();
                connection->send(
                    boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY)),
                    boost::beast::http::status::ok);
            }
        }
        catch (boost::system::system_error const&)
        {
            // system_error thrown when json parsing failed
            rpcEngine_->notifyBadSyntax();
            connection->send(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX)),
                boost::beast::http::status::ok);
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
    operator()(boost::beast::error_code _, std::shared_ptr<Server::ConnectionBase> const& connection)
    {
        if (auto manager = subscriptions_.lock(); manager)
            manager->cleanup(connection);
    }

private:
    void
    handleRequest(
        boost::asio::yield_context& yc,
        boost::json::object&& request,
        std::shared_ptr<Server::ConnectionBase> connection)
    {
        log_.info() << connection->tag() << (connection->upgraded ? "ws" : "http")
                    << " received request from work queue: " << request << " ip = " << connection->clientIp;

        auto const id = request.contains("id") ? request.at("id") : nullptr;

        auto const composeError = [&](auto const& error) -> boost::json::object {
            auto e = RPC::makeError(error);
            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;

            if (connection->upgraded)
                return e;
            else
                return boost::json::object{{"result", e}};
        };

        try
        {
            auto const range = backend_->fetchLedgerRange();
            // for the error happened before the handler, we don't attach the clio warning
            if (!range)
            {
                rpcEngine_->notifyNotReady();
                return connection->send(
                    boost::json::serialize(composeError(RPC::RippledError::rpcNOT_READY)),
                    boost::beast::http::status::ok);
            }

            auto context = connection->upgraded
                ? RPC::make_WsContext(
                      yc, request, connection, tagFactory_.with(connection->tag()), *range, connection->clientIp)
                : RPC::make_HttpContext(yc, request, tagFactory_.with(connection->tag()), *range, connection->clientIp);

            if (!context)
            {
                perfLog_.warn() << connection->tag() << "Could not create RPC context";
                log_.warn() << connection->tag() << "Could not create RPC context";

                rpcEngine_->notifyBadSyntax();
                return connection->send(
                    boost::json::serialize(composeError(RPC::RippledError::rpcBAD_SYNTAX)),
                    boost::beast::http::status::ok);
            }

            auto [v, timeDiff] = util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            RPC::logDuration(*context, us);

            boost::json::object response;
            if (auto const status = std::get_if<RPC::Status>(&v))
            {
                // note: error statuses are counted/notified in buildResponse itself
                response = std::move(composeError(*status));
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

                // for ws , there is additional field "status" in response
                // otherwise , the "status" is in the "result" field
                if (connection->upgraded)
                {
                    if (!id.is_null())
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
            connection->send(boost::json::serialize(response), boost::beast::http::status::ok);
        }
        catch (std::exception const& ex)
        {
            // note: while we are catching this in buildResponse too, this is here to make sure
            // that any other code that may throw is outside of buildResponse is also worked around.
            perfLog_.error() << connection->tag() << "Caught exception: " << ex.what();
            log_.error() << connection->tag() << "Caught exception: " << ex.what();

            rpcEngine_->notifyInternalError();

            return connection->send(
                boost::json::serialize(composeError(RPC::RippledError::rpcINTERNAL)),
                boost::beast::http::status::internal_server_error);
        }
    }
};
