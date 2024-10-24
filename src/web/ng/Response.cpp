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
#include <string_view>
#include <type_traits>
#include <utility>

namespace http = boost::beast::http;
namespace web::ng {

namespace {

std::string_view
asString(Response::HttpData::ContentType type)
{
    switch (type) {
        case Response::HttpData::ContentType::TextHtml:
            return "text/html";
        case Response::HttpData::ContentType::ApplicationJson:
            return "application/json";
    }
    ASSERT(false, "Unknown content type");
    std::unreachable();
}

template <typename MessageType>
std::optional<Response::HttpData>
makeHttpData(http::status status, Request const& request)
{
    if (request.isHttp()) {
        auto const& httpRequest = request.asHttpRequest()->get();
        auto constexpr contentType = std::is_same_v<std::remove_cvref_t<MessageType>, std::string>
            ? Response::HttpData::ContentType::TextHtml
            : Response::HttpData::ContentType::ApplicationJson;
        return Response::HttpData{
            .status = status,
            .contentType = contentType,
            .keepAlive = httpRequest.keep_alive(),
            .version = httpRequest.version()
        };
    }
    return std::nullopt;
}
}  // namespace

Response::Response(boost::beast::http::status status, std::string message, Request const& request)
    : message_(std::move(message)), httpData_{makeHttpData<decltype(message)>(status, request)}
{
}

Response::Response(boost::beast::http::status status, boost::json::object const& message, Request const& request)
    : message_(boost::json::serialize(message)), httpData_{makeHttpData<decltype(message)>(status, request)}
{
}

std::string const&
Response::message() const
{
    return message_;
}

http::response<http::string_body>
Response::intoHttpResponse() &&
{
    ASSERT(httpData_.has_value(), "Response must have http data to be converted into http response");

    http::response<http::string_body> result{httpData_->status, httpData_->version};
    result.set(http::field::server, fmt::format("clio-server-{}", util::build::getClioVersionString()));
    result.set(http::field::content_type, asString(httpData_->contentType));
    result.keep_alive(httpData_->keepAlive);
    result.body() = std::move(message_);
    result.prepare_payload();
    return result;
}

boost::asio::const_buffer
Response::asConstBuffer() const&
{
    ASSERT(not httpData_.has_value(), "Losing existing http data");
    return boost::asio::buffer(message_.data(), message_.size());
}

}  // namespace web::ng
