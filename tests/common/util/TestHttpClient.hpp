//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <boost/asio/ssl/verify_context.hpp>
#include <boost/beast/http/field.hpp>

#include <string>
#include <vector>

struct WebHeader {
    WebHeader(boost::beast::http::field name, std::string value);

    boost::beast::http::field name;
    std::string value;
};

struct HttpSyncClient {
    static std::string
    post(
        std::string const& host,
        std::string const& port,
        std::string const& body,
        std::vector<WebHeader> additionalHeaders = {}
    );

    static std::string
    get(std::string const& host,
        std::string const& port,
        std::string const& body,
        std::string const& target,
        std::vector<WebHeader> additionalHeaders = {});
};

struct HttpsSyncClient {
    static bool
    verify_certificate(bool /* preverified */, boost::asio::ssl::verify_context& /* ctx */);

    static std::string
    syncPost(std::string const& host, std::string const& port, std::string const& body);
};
