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

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/verify_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
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

class HttpAsyncClient {
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;

public:
    HttpAsyncClient(boost::asio::io_context& ioContext);

    std::optional<boost::system::error_code>
    connect(
        std::string_view host,
        std::string_view port,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout
    );

    std::optional<boost::system::error_code>
    send(
        boost::beast::http::request<boost::beast::http::string_body> request,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout
    );

    std::expected<boost::beast::http::response<boost::beast::http::string_body>, boost::system::error_code>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    gracefulShutdown();
    void
    disconnect();
};
