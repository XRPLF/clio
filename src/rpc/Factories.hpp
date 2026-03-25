#pragma once

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/APIVersion.hpp"
#include "util/Taggable.hpp"
#include "web/Context.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <expected>
#include <functional>
#include <string>

/*
 * This file contains various classes necessary for executing RPC handlers.
 * Context gives the handlers access to various other parts of the application Status is used to
 * report errors. And lastly, there are various functions for making Contexts, Statuses and
 * serializing Status to JSON. This file is meant to contain any class or function that code outside
 * of the rpc folder needs to use. For helper functions or classes used within the rpc folder, use
 * RPCHelpers.h.
 */
namespace rpc {

/**
 * @brief A factory function that creates a Websocket context.
 *
 * @param yc The coroutine context
 * @param request The request as JSON object
 * @param session The subscription context
 * @param tagFactory A factory that provides tags to track requests
 * @param range The ledger range that is available at request time
 * @param clientIp The IP address of the connected client
 * @param apiVersionParser A parser that is used to parse out the "api_version" field
 * @param isAdmin Whether the request has admin privileges
 * @return A Websocket context or error Status
 */
std::expected<web::Context, Status>
makeWsContext(
    boost::asio::yield_context yc,
    boost::json::object const& request,
    web::SubscriptionContextPtr session,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    std::string const& clientIp,
    std::reference_wrapper<APIVersionParser const> apiVersionParser,
    bool isAdmin
);

/**
 * @brief A factory function that creates a HTTP context.
 *
 * @param yc The coroutine context
 * @param request The request as JSON object
 * @param tagFactory A factory that provides tags to track requests
 * @param range The ledger range that is available at request time
 * @param clientIp The IP address of the connected client
 * @param apiVersionParser A parser that is used to parse out the "api_version" field
 * @param isAdmin Whether the connection has admin privileges
 * @return A HTTP context or error Status
 */
std::expected<web::Context, Status>
makeHttpContext(
    boost::asio::yield_context yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    std::string const& clientIp,
    std::reference_wrapper<APIVersionParser const> apiVersionParser,
    bool isAdmin
);

}  // namespace rpc
