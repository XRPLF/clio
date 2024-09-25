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

#pragma once

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace web::ng {

class Request {
public:
    using HttpHeaders = boost::beast::http::request<boost::beast::http::string_body>::header_type;

private:
    struct WsData {
        std::string request;
        std::reference_wrapper<HttpHeaders const> headers_;
    };

    std::variant<boost::beast::http::request<boost::beast::http::string_body>, WsData> data_;

public:
    explicit Request(boost::beast::http::request<boost::beast::http::string_body> request);
    Request(std::string request, HttpHeaders const& headers);

    enum class HttpMethod { GET, POST, WEBSOCKET, UNSUPPORTED };

    HttpMethod
    httpMethod() const;

    std::string const&
    target() const;

    std::optional<std::string_view>
    headerValue(boost::beast::http::field headerName) const;
    std::optional<std::string_view>
    headerValue(std::string const& headerName) const;
};

}  // namespace web::ng
