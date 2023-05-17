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
#include <log/Logger.h>
#include <rpc/Errors.h>
#include <webserver/Context.h>
#include <webserver2/details/WsBase.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <fmt/core.h>

#include <chrono>
#include <optional>
#include <string>

/*
 * This file contains various classes necessary for executing RPC handlers.
 * Context gives the handlers access to various other parts of the application Status is used to report errors.
 * And lastly, there are various functions for making Contexts, Statuses and serializing Status to JSON.
 * This file is meant to contain any class or function that code outside of the rpc folder needs to use. For helper
 * functions or classes used within the rpc folder, use RPCHelpers.h.
 */

class WsBase;

namespace RPC {

std::optional<Web::Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<Server::ConnectionBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    std::string const& clientIp);

std::optional<Web::Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    std::string const& clientIp);

}  // namespace RPC
