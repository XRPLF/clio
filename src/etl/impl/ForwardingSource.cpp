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

#include "etl/impl/ForwardingSource.hpp"

#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/version.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace etl::impl {

ForwardingSource::ForwardingSource(
    std::string ip,
    std::string wsPort,
    std::chrono::steady_clock::duration connectionTimeout
)
    : log_(fmt::format("ForwardingSource[{}:{}]", ip, wsPort)), connectionBuilder_(std::move(ip), std::move(wsPort))
{
    connectionBuilder_.setConnectionTimeout(connectionTimeout)
        .addHeader(
            {boost::beast::http::field::user_agent, fmt::format("{} websocket-client-coro", BOOST_BEAST_VERSION_STRING)}
        );
}

std::optional<boost::json::object>
ForwardingSource::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& forwardToRippledClientIp,
    boost::asio::yield_context yield
) const
{
    auto connectionBuilder = connectionBuilder_;
    if (forwardToRippledClientIp) {
        connectionBuilder.addHeader(
            {boost::beast::http::field::forwarded, fmt::format("for={}", *forwardToRippledClientIp)}
        );
    }
    auto expectedConnection = connectionBuilder.connect(yield);
    if (not expectedConnection) {
        return std::nullopt;
    }
    auto& connection = expectedConnection.value();

    auto writeError = connection->write(boost::json::serialize(request), yield);
    if (writeError) {
        return std::nullopt;
    }

    auto response = connection->read(yield);
    if (not response) {
        return std::nullopt;
    }

    auto parsedResponse = boost::json::parse(response.value());
    if (not parsedResponse.is_object()) {
        LOG(log_.error()) << "Error parsing response from rippled: " << response.value();
        return std::nullopt;
    }

    auto responseObject = parsedResponse.as_object();
    responseObject["forwarded"] = true;
    return responseObject;
}

}  // namespace etl::impl
