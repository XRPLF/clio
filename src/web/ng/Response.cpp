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

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace web::ng {

namespace {

std::string_view
asString(Response::HttpData::ContentType type)
{
    switch (type) {
        case Response::HttpData::ContentType::TEXT_HTML:
            return "text/html";
        case Response::HttpData::ContentType::APPLICATION_JSON:
            return "application/json";
    }
    ASSERT(false, "Unknown content type");
    std::unreachable();
}

}  // namespace

Response::Response(std::string message, std::optional<HttpData> httpData)
    : message_(std::move(message)), httpData_(httpData)
{
}

boost::beast::http::response<boost::beast::http::string_body>
Response::intoHttpResponse() &&
{
    ASSERT(httpData_.has_value(), "Response must have http data to be converted into http response");

    boost::beast::http::response<boost::beast::http::string_body> result{httpData_->status, httpData_->version};
    result.set(boost::beast::http::field::server, "clio-server-" + util::build::getClioVersionString());
    result.set(boost::beast::http::field::content_type, asString(httpData_->contentType));
    result.keep_alive(httpData_->keepAlive);
    result.body() = std::move(message_);
    result.prepare_payload();
    return result;
}

boost::asio::const_buffer
Response::asConstBuffer() const&
{
    ASSERT(not httpData_.has_value(), "Loosing existing http data");
    return boost::asio::buffer(message_.data(), message_.size());
}

}  // namespace web::ng