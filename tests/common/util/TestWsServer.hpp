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

#include "util/requests/Types.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class TestWsConnection {
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;

public:
    using SendCallback = std::function<void()>;
    using ReceiveCallback = std::function<void(std::string)>;

    TestWsConnection(boost::beast::websocket::stream<boost::beast::tcp_stream> wsStream);

    TestWsConnection(TestWsConnection&& other);

    // returns error message if error occurs
    std::optional<std::string>
    send(std::string const& message, boost::asio::yield_context yield);

    // returns nullopt if the connection is closed
    std::optional<std::string>
    receive(boost::asio::yield_context yield);

    std::optional<std::string>
    close(boost::asio::yield_context yield);
};
using TestWsConnectionPtr = std::unique_ptr<TestWsConnection>;

class TestWsServer {
    boost::asio::ip::tcp::acceptor acceptor_;

public:
    TestWsServer(boost::asio::io_context& context, std::string const& host, int port);

    std::expected<TestWsConnection, util::requests::RequestError>
    acceptConnection(boost::asio::yield_context yield);

    void
    acceptConnectionAndDropIt(boost::asio::yield_context yield);

    boost::asio::ip::tcp::socket
    acceptConnectionWithoutHandshake(boost::asio::yield_context yield);
};
