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

#include <rpc/Factories.h>

using namespace std;
using namespace clio;

namespace RPC {

optional<Web::Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    shared_ptr<Server::ConnectionBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    string const& clientIp)
{
    boost::json::value commandValue = nullptr;
    if (!request.contains("command") && request.contains("method"))
        commandValue = request.at("method");
    else if (request.contains("command") && !request.contains("method"))
        commandValue = request.at("command");

    if (!commandValue.is_string())
        return {};

    string command = commandValue.as_string().c_str();
    return make_optional<Web::Context>(yc, command, 1, request, session, tagFactory, range, clientIp);
}

optional<Web::Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    string const& clientIp)
{
    if (!request.contains("method") || !request.at("method").is_string())
        return {};

    string const& command = request.at("method").as_string().c_str();

    if (command == "subscribe" || command == "unsubscribe")
        return {};

    if (!request.at("params").is_array())
        return {};

    boost::json::array const& array = request.at("params").as_array();

    if (array.size() != 1)
        return {};

    if (!array.at(0).is_object())
        return {};

    return make_optional<Web::Context>(yc, command, 1, array.at(0).as_object(), nullptr, tagFactory, range, clientIp);
}

}  // namespace RPC
