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

#include "util/TestWsServer.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

TestWsConnection::TestWsConnection(asio::ip::tcp::socket socket, boost::asio::yield_context yield)
    : ws_(std::move(socket))
{
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    beast::error_code errorCode;
    ws_.async_accept(yield[errorCode]);
    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
}

void
TestWsConnection::send(std::string const& message, boost::asio::yield_context yield)
{
    beast::error_code errorCode;
    ws_.async_write(asio::buffer(message), yield[errorCode]);
    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
}

std::optional<std::string>
TestWsConnection::receive(boost::asio::yield_context yield)
{
    beast::error_code errorCode;
    beast::flat_buffer buffer;

    ws_.async_read(buffer, yield[errorCode]);
    if (errorCode == websocket::error::closed)
        return std::nullopt;

    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
    return beast::buffers_to_string(buffer.data());
}

TestWsServer::TestWsServer(asio::io_context& context, std::string host, int port)
    : acceptor_(context, asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), port))
{
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);

    // Accept a connection
    asio::spawn(acceptor_.get_executor(), [&](auto yield) { acceptConnection(yield); });
}

TestWsConnection
TestWsServer::acceptConnection(asio::yield_context yield)
{
    beast::error_code errorCode;
    asio::ip::tcp::socket socket_(acceptor_.get_executor());
    acceptor_.async_accept(socket_, yield[errorCode]);
    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
    return TestWsConnection(std::move(socket_), yield);
}
