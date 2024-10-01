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

#include "web/ng/Request.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace web::ng {

namespace {

template <typename HeadersType, typename HeaderNameType>
std::optional<std::string_view>
getHeaderValue(HeadersType const& headers, HeaderNameType const& headerName)
{
    auto it = headers.find(headerName);
    if (it == headers.end())
        return std::nullopt;
    return it->value();
}

}  // namespace

Request::Request(boost::beast::http::request<boost::beast::http::string_body> request) : data_{std::move(request)}
{
}

Request::Request(std::string request, HttpHeaders const& headers)
    : data_{WsData{.request = std::move(request), .headers = headers}}
{
}

Request::Method
Request::method() const
{
    if (not isHttp())
        return Method::WEBSOCKET;

    switch (httpRequest().method()) {
        case boost::beast::http::verb::get:
            return Method::GET;
        case boost::beast::http::verb::post:
            return Method::POST;
        default:
            return Method::UNSUPPORTED;
    }
}

bool
Request::isHttp() const
{
    return std::holds_alternative<HttpRequest>(data_);
}

std::optional<std::reference_wrapper<boost::beast::http::request<boost::beast::http::string_body> const>>
Request::asHttpRequest() const
{
    if (not isHttp())
        return std::nullopt;

    return httpRequest();
}

std::optional<std::string_view>
Request::target() const
{
    if (not isHttp())
        return std::nullopt;

    return httpRequest().target();
}

std::optional<std::string_view>
Request::headerValue(boost::beast::http::field headerName) const
{
    if (not isHttp())
        return getHeaderValue(std::get<WsData>(data_).headers.get(), headerName);

    return getHeaderValue(httpRequest(), headerName);
}

std::optional<std::string_view>
Request::headerValue(std::string const& headerName) const
{
    if (not isHttp())
        return getHeaderValue(std::get<WsData>(data_).headers.get(), headerName);

    return getHeaderValue(httpRequest(), headerName);
}

Request::HttpRequest const&
Request::httpRequest() const
{
    return std::get<HttpRequest>(data_);
}

}  // namespace web::ng
