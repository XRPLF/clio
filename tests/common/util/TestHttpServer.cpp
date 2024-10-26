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

#include "util/TestHttpServer.hpp"

#include "util/Assert.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/read.hpp>  // IWYU pragma: keep
#include <boost/beast/http/string_body.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <expected>
#include <string>
#include <utility>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace {

void
doSession(
    beast::tcp_stream stream,
    TestHttpServer::RequestHandler requestHandler,
    asio::yield_context yield,
    bool const allowToFail
)
{
    beast::error_code errorCode;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    // Set the timeout.
    stream.expires_after(std::chrono::seconds(5));

    // Read a request
    http::request<http::string_body> req;
    http::async_read(stream, buffer, req, yield[errorCode]);
    if (errorCode == http::error::end_of_stream)
        return;

    if (allowToFail and errorCode)
        return;

    ASSERT_FALSE(errorCode) << errorCode.message();

    auto response = requestHandler(req);

    if (not response)
        return;

    bool const keep_alive = response->keep_alive();

    http::message_generator messageGenerator{std::move(response).value()};

    // Send the response
    beast::async_write(stream, std::move(messageGenerator), yield[errorCode]);

    if (allowToFail and errorCode)
        return;

    ASSERT_FALSE(errorCode) << errorCode.message();

    if (!keep_alive) {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        return;
    }

    // Send a TCP shutdown
    stream.socket().shutdown(tcp::socket::shutdown_send, errorCode);

    // At this point the connection is closed gracefully
}

}  // namespace

TestHttpServer::TestHttpServer(boost::asio::io_context& context, std::string host) : acceptor_(context)
{
    boost::asio::ip::tcp::resolver resolver{context};
    auto const results = resolver.resolve(host, "0");
    ASSERT(!results.empty(), "Failed to resolve host");
    boost::asio::ip::tcp::endpoint const& endpoint = results.begin()->endpoint();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);
}

std::expected<boost::asio::ip::tcp::socket, boost::system::error_code>
TestHttpServer::accept(boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    tcp::socket socket(this->acceptor_.get_executor());
    acceptor_.async_accept(socket, yield[errorCode]);
    if (errorCode)
        return std::unexpected{errorCode};
    return socket;
}

void
TestHttpServer::handleRequest(TestHttpServer::RequestHandler handler, bool const allowToFail)
{
    boost::asio::spawn(
        acceptor_.get_executor(),
        [this, allowToFail, handler = std::move(handler)](asio::yield_context yield) mutable {
            boost::beast::error_code errorCode;
            tcp::socket socket(this->acceptor_.get_executor());
            acceptor_.async_accept(socket, yield[errorCode]);

            if (allowToFail and errorCode)
                return;

            [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();

            doSession(beast::tcp_stream{std::move(socket)}, std::move(handler), yield, allowToFail);
        },
        boost::asio::detached
    );
}

std::string
TestHttpServer::port() const
{
    return std::to_string(acceptor_.local_endpoint().port());
}
