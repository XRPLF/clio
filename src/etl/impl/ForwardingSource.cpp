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

#include "rpc/Errors.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/version.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>

#include <chrono>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace etl::impl {

ForwardingSource::ForwardingSource(
    std::string ip,
    std::string wsPort,
    std::chrono::steady_clock::duration forwardingTimeout,
    std::chrono::steady_clock::duration connectionTimeout
)
    : log_(fmt::format("ForwardingSource[{}:{}]", ip, wsPort))
    , connectionBuilder_(std::move(ip), std::move(wsPort))
    , forwardingTimeout_{forwardingTimeout}
{
    connectionBuilder_.setConnectionTimeout(connectionTimeout)
        .addHeader(
            {boost::beast::http::field::user_agent, fmt::format("{} websocket-client-coro", BOOST_BEAST_VERSION_STRING)}
        );
}

std::expected<boost::json::object, rpc::ClioError>
ForwardingSource::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& forwardToRippledClientIp,
    std::string_view xUserValue,
    boost::asio::yield_context yield
) const
{
    auto connectionBuilder = connectionBuilder_;
    if (forwardToRippledClientIp) {
        connectionBuilder.addHeader(
            {boost::beast::http::field::forwarded, fmt::format("for={}", *forwardToRippledClientIp)}
        );
    }

    connectionBuilder.addHeader({"X-User", std::string{xUserValue}});

    auto expectedConnection = connectionBuilder.connect(yield);
    if (not expectedConnection) {
        LOG(log_.debug()) << "Couldn't connect to rippled to forward request.";
        return std::unexpected{rpc::ClioError::etlCONNECTION_ERROR};
    }
    auto& connection = expectedConnection.value();

    auto writeError = connection->write(boost::json::serialize(request), yield, forwardingTimeout_);
    if (writeError) {
        LOG(log_.debug()) << "Error sending request to rippled to forward request.";
        return std::unexpected{rpc::ClioError::etlREQUEST_ERROR};
    }

    auto response = connection->read(yield, forwardingTimeout_);
    if (not response) {
        if (auto errorCode = response.error().errorCode();
            errorCode.has_value() and errorCode->value() == boost::system::errc::timed_out) {
            LOG(log_.debug()) << "Request to rippled timed out";
            return std::unexpected{rpc::ClioError::etlREQUEST_TIMEOUT};
        }
        LOG(log_.debug()) << "Error sending request to rippled to forward request.";
        return std::unexpected{rpc::ClioError::etlREQUEST_ERROR};
    }

    boost::json::value parsedResponse;
    try {
        parsedResponse = boost::json::parse(*response);
        if (not parsedResponse.is_object())
            throw std::runtime_error("response is not an object");
    } catch (std::exception const& e) {
        LOG(log_.debug()) << "Error parsing response from rippled: " << e.what() << ". Response: " << *response;
        return std::unexpected{rpc::ClioError::etlINVALID_RESPONSE};
    }

    auto responseObject = parsedResponse.as_object();
    responseObject["forwarded"] = true;

    return responseObject;
}

}  // namespace etl::impl
