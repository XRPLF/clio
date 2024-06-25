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

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/Factories.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/impl/APIVersionParser.hpp"
#include "util/JsonUtils.hpp"
#include "util/Profiler.hpp"
#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/impl/ErrorHandling.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/system/system_error.hpp>
#include <xrpl/protocol/jss.h>

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string>
#include <utility>

namespace web {

/**
 * @brief The server handler for RPC requests called by web server.
 *
 * Note: see @ref web::SomeServerHandler concept
 */
template <typename RPCEngineType, typename ETLType>
class RPCServerHandler {
    std::shared_ptr<BackendInterface const> const backend_;
    std::shared_ptr<RPCEngineType> const rpcEngine_;
    std::shared_ptr<ETLType const> const etl_;
    util::TagDecoratorFactory const tagFactory_;
    rpc::impl::ProductionAPIVersionParser apiVersionParser_;  // can be injected if needed

    util::Logger log_{"RPC"};
    util::Logger perfLog_{"Performance"};

public:
    /**
     * @brief Create a new server handler.
     *
     * @param config Clio config to use
     * @param backend The backend to use
     * @param rpcEngine The RPC engine to use
     * @param etl The ETL to use
     */
    RPCServerHandler(
        util::Config const& config,
        std::shared_ptr<BackendInterface const> const& backend,
        std::shared_ptr<RPCEngineType> const& rpcEngine,
        std::shared_ptr<ETLType const> const& etl
    )
        : backend_(backend)
        , rpcEngine_(rpcEngine)
        , etl_(etl)
        , tagFactory_(config)
        , apiVersionParser_(config.sectionOr("api_version", {}))
    {
    }

    /**
     * @brief The callback when server receives a request.
     *
     * @param request The request
     * @param connection The connection
     */
    void
    operator()(std::string const& request, std::shared_ptr<web::ConnectionBase> const& connection)
    {
        try {
            auto req = boost::json::parse(request).as_object();
            LOG(perfLog_.debug()) << connection->tag() << "Adding to work queue";

            if (not connection->upgraded and shouldReplaceParams(req))
                req[JS(params)] = boost::json::array({boost::json::object{}});

            if (!rpcEngine_->post(
                    [this, request = std::move(req), connection](boost::asio::yield_context yield) mutable {
                        handleRequest(yield, std::move(request), connection);
                    },
                    connection->clientIp
                )) {
                rpcEngine_->notifyTooBusy();
                web::impl::ErrorHelper(connection).sendTooBusyError();
            }
        } catch (boost::system::system_error const& ex) {
            // system_error thrown when json parsing failed
            rpcEngine_->notifyBadSyntax();
            web::impl::ErrorHelper(connection).sendJsonParsingError();
            LOG(log_.warn()) << "Error parsing JSON: " << ex.what() << ". For request: " << request;
        } catch (std::invalid_argument const& ex) {
            // thrown when json parses something that is not an object at top level
            rpcEngine_->notifyBadSyntax();
            LOG(log_.warn()) << "Invalid argument error: " << ex.what() << ". For request: " << request;
            web::impl::ErrorHelper(connection).sendJsonParsingError();
        } catch (std::exception const& ex) {
            LOG(perfLog_.error()) << connection->tag() << "Caught exception: " << ex.what();
            rpcEngine_->notifyInternalError();
            throw;
        }
    }

private:
    void
    handleRequest(
        boost::asio::yield_context yield,
        boost::json::object&& request,
        std::shared_ptr<web::ConnectionBase> const& connection
    )
    {
        LOG(log_.info()) << connection->tag() << (connection->upgraded ? "ws" : "http")
                         << " received request from work queue: " << util::removeSecret(request)
                         << " ip = " << connection->clientIp;

        try {
            auto const range = backend_->fetchLedgerRange();
            if (!range) {
                // for error that happened before the handler, we don't attach any warnings
                rpcEngine_->notifyNotReady();
                web::impl::ErrorHelper(connection, std::move(request)).sendNotReadyError();

                return;
            }

            auto const context = [&] {
                if (connection->upgraded) {
                    return rpc::make_WsContext(
                        yield,
                        request,
                        connection,
                        tagFactory_.with(connection->tag()),
                        *range,
                        connection->clientIp,
                        std::cref(apiVersionParser_)
                    );
                }
                return rpc::make_HttpContext(
                    yield,
                    request,
                    tagFactory_.with(connection->tag()),
                    *range,
                    connection->clientIp,
                    std::cref(apiVersionParser_),
                    connection->isAdmin()
                );
            }();

            if (!context) {
                auto const err = context.error();
                LOG(perfLog_.warn()) << connection->tag() << "Could not create Web context: " << err;
                LOG(log_.warn()) << connection->tag() << "Could not create Web context: " << err;

                // we count all those as BadSyntax - as the WS path would.
                // Although over HTTP these will yield a 400 status with a plain text response (for most).
                rpcEngine_->notifyBadSyntax();
                web::impl::ErrorHelper(connection, std::move(request)).sendError(err);

                return;
            }

            auto [result, timeDiff] = util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            rpc::logDuration(*context, us);

            boost::json::object response;

            if (auto const status = std::get_if<rpc::Status>(&result.response)) {
                // note: error statuses are counted/notified in buildResponse itself
                response = web::impl::ErrorHelper(connection, request).composeError(*status);
                auto const responseStr = boost::json::serialize(response);

                LOG(perfLog_.debug()) << context->tag() << "Encountered error: " << responseStr;
                LOG(log_.debug()) << context->tag() << "Encountered error: " << responseStr;
            } else {
                // This can still technically be an error. Clio counts forwarded requests as successful.
                rpcEngine_->notifyComplete(context->method, us);

                auto& json = std::get<boost::json::object>(result.response);
                auto const isForwarded =
                    json.contains("forwarded") && json.at("forwarded").is_bool() && json.at("forwarded").as_bool();

                if (isForwarded)
                    json.erase("forwarded");

                // if the result is forwarded - just use it as is
                // if forwarded request has error, for http, error should be in "result"; for ws, error should
                // be at top
                if (isForwarded && (json.contains(JS(result)) || connection->upgraded)) {
                    for (auto const& [k, v] : json)
                        response.insert_or_assign(k, v);
                } else {
                    response[JS(result)] = json;
                }

                if (isForwarded)
                    response["forwarded"] = true;

                // for ws there is an additional field "status" in the response,
                // otherwise the "status" is in the "result" field
                if (connection->upgraded) {
                    auto const appendFieldIfExist = [&](auto const& field) {
                        if (request.contains(field) and not request.at(field).is_null())
                            response[field] = request.at(field);
                    };

                    appendFieldIfExist(JS(id));
                    appendFieldIfExist(JS(api_version));

                    if (!response.contains(JS(error)))
                        response[JS(status)] = JS(success);

                    response[JS(type)] = JS(response);
                } else {
                    if (response.contains(JS(result)) && !response[JS(result)].as_object().contains(JS(error)))
                        response[JS(result)].as_object()[JS(status)] = JS(success);
                }
            }

            boost::json::array warnings = std::move(result.warnings);
            warnings.emplace_back(rpc::makeWarning(rpc::warnRPC_CLIO));

            if (etl_->lastCloseAgeSeconds() >= 60)
                warnings.emplace_back(rpc::makeWarning(rpc::warnRPC_OUTDATED));

            response["warnings"] = warnings;
            connection->send(boost::json::serialize(response));
        } catch (std::exception const& ex) {
            // note: while we are catching this in buildResponse too, this is here to make sure
            // that any other code that may throw is outside of buildResponse is also worked around.
            LOG(perfLog_.error()) << connection->tag() << "Caught exception: " << ex.what();
            LOG(log_.error()) << connection->tag() << "Caught exception: " << ex.what();

            rpcEngine_->notifyInternalError();
            web::impl::ErrorHelper(connection, std::move(request)).sendInternalError();

            return;
        }
    }

    bool
    shouldReplaceParams(boost::json::object const& req) const
    {
        auto const hasParams = req.contains(JS(params));
        auto const paramsIsArray = hasParams and req.at(JS(params)).is_array();
        auto const paramsIsEmptyString =
            hasParams and req.at(JS(params)).is_string() and req.at(JS(params)).as_string().empty();
        auto const paramsIsEmptyObject =
            hasParams and req.at(JS(params)).is_object() and req.at(JS(params)).as_object().empty();
        auto const paramsIsNull = hasParams and req.at(JS(params)).is_null();
        auto const arrayIsEmpty = paramsIsArray and req.at(JS(params)).as_array().empty();
        auto const arrayIsNotEmpty = paramsIsArray and not req.at(JS(params)).as_array().empty();
        auto const firstArgIsNull = arrayIsNotEmpty and req.at(JS(params)).as_array().at(0).is_null();
        auto const firstArgIsEmptyString = arrayIsNotEmpty and req.at(JS(params)).as_array().at(0).is_string() and
            req.at(JS(params)).as_array().at(0).as_string().empty();

        // Note: all this compatibility dance is to match `rippled` as close as possible
        return not hasParams or paramsIsEmptyString or paramsIsNull or paramsIsEmptyObject or arrayIsEmpty or
            firstArgIsEmptyString or firstArgIsNull;
    }
};

}  // namespace web
