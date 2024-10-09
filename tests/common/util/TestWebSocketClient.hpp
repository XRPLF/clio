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

#include "util/TestHttpClient.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

class WebSocketSyncClient {
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::resolver resolver_{ioc_};
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_{ioc_};

public:
    void
    connect(std::string const& host, std::string const& port, std::vector<WebHeader> additionalHeaders = {});

    void
    disconnect();

    std::string
    syncPost(std::string const& body);
};

class WebSocketAsyncClient {
    boost::beast::websocket::stream<boost::beast::tcp_stream> stream_;

public:
    WebSocketAsyncClient(boost::asio::io_context& ioContext);

    std::optional<boost::system::error_code>
    connect(
        boost::asio::yield_context yield,
        std::string const& host,
        std::string const& port,
        std::chrono::steady_clock::duration timeout,
        std::vector<WebHeader> additionalHeaders = {}
    );

    std::optional<boost::system::error_code>
    send(boost::asio::yield_context yield, std::string const& message, std::chrono::steady_clock::duration timeout);

    std::expected<std::string, boost::system::error_code>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    gracefulClose(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    close();
};

class WebServerSslSyncClient {
    boost::asio::io_context ioc_;
    std::optional<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>> ws_;

public:
    void
    connect(std::string const& host, std::string const& port);

    void
    disconnect();

    std::string
    syncPost(std::string const& body);
};
