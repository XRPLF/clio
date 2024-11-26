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

#include "web/ng/Response.hpp"

#include "util/Assert.hpp"
#include "util/OverloadSet.hpp"
#include "util/build/Build.hpp"
#include "web/ng/Request.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace http = boost::beast::http;

namespace web::ng {

namespace {

template <typename T>
consteval bool
isString()
{
    return std::is_same_v<T, std::string>;
}

http::response<http::string_body>
prepareResponse(http::response<http::string_body> response, http::request<http::string_body> const& request)
{
    response.set(http::field::server, fmt::format("clio-server-{}", util::build::getClioVersionString()));
    response.keep_alive(request.keep_alive());
    response.prepare_payload();
    return response;
}

template <typename MessageType>
std::variant<http::response<http::string_body>, std::string>
makeData(http::status status, MessageType message, Request const& request)
{
    std::string body;
    if constexpr (isString<MessageType>()) {
        body = std::move(message);
    } else {
        body = boost::json::serialize(message);
    }

    if (not request.isHttp())
        return body;

    auto const& httpRequest = request.asHttpRequest()->get();
    std::string const contentType = isString<MessageType>() ? "text/html" : "application/json";

    http::response<http::string_body> result{status, httpRequest.version(), std::move(body)};
    result.set(http::field::content_type, contentType);
    return prepareResponse(std::move(result), httpRequest);
}

}  // namespace

Response::Response(boost::beast::http::status status, std::string message, Request const& request)
    : data_{makeData(status, std::move(message), request)}
{
}

Response::Response(boost::beast::http::status status, boost::json::object const& message, Request const& request)
    : data_{makeData(status, message, request)}
{
}

Response::Response(boost::beast::http::response<boost::beast::http::string_body> response, Request const& request)
{
    ASSERT(request.isHttp(), "Request must be HTTP to construct response from HTTP response");
    data_ = prepareResponse(std::move(response), request.asHttpRequest()->get());
}

std::string const&
Response::message() const
{
    return std::visit(
        util::OverloadSet{
            [](http::response<http::string_body> const& response) -> std::string const& { return response.body(); },
            [](std::string const& message) -> std::string const& { return message; },
        },
        data_
    );
}

http::response<http::string_body>
Response::intoHttpResponse() &&
{
    ASSERT(std::holds_alternative<http::response<http::string_body>>(data_), "Response must contain HTTP data");

    return std::move(std::get<http::response<http::string_body>>(data_));
}

boost::asio::const_buffer
Response::asWsResponse() const&
{
    ASSERT(std::holds_alternative<std::string>(data_), "Response must contain WebSocket data");
    auto const& message = std::get<std::string>(data_);
    return boost::asio::buffer(message.data(), message.size());
}

}  // namespace web::ng
