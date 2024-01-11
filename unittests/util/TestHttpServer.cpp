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

#include "util/TestHttpServer.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/string_body.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace asio = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

namespace {

void
doSession(beast::tcp_stream stream, TestHttpServer::RequestHandler& requestHandler, asio::yield_context yield)
{
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    for (;;) {
        // Set the timeout.
        stream.expires_after(std::chrono::seconds(5));

        // Read a request
        http::request<http::string_body> req;
        http::async_read(stream, buffer, req, yield[ec]);
        if (ec == http::error::end_of_stream)
            break;

        if (ec)
            throw std::runtime_error(ec.what());

        auto response = requestHandler(req);

        if (not response)
            break;

        bool const keep_alive = response->keep_alive();

        http::message_generator messageGenerator{std::move(response).value()};

        // Send the response
        beast::async_write(stream, std::move(messageGenerator), yield[ec]);

        if (ec)
            throw std::runtime_error(ec.what());

        if (!keep_alive) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Send a TCP shutdown
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

}  // namespace

TestHttpServer::TestHttpServer(
    boost::asio::io_context& context,
    std::string host,
    int const port,
    RequestHandler handler
)
    : acceptor_(context), handler_(std::move(handler))
{
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);

    boost::beast::error_code errorCode;
    for (;;) {
        tcp::socket socket(context);
        boost::asio::spawn(context, [this, &socket, &errorCode](asio::yield_context yield) {
            acceptor_.async_accept(socket, yield[errorCode]);
        });

        if (errorCode)
            throw std::runtime_error(errorCode.what());

        boost::asio::spawn(
            context,
            [this, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                doSession(beast::tcp_stream{std::move(socket)}, handler_, yield);
            },
            // we ignore the result of the session,
            // most errors are handled with error_code
            boost::asio::detached
        );
    }
}
