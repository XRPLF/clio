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

#include "util/TestWebSocketClient.hpp"

#include "util/TestHttpClient.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <openssl/err.h>
#include <openssl/tls1.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

void
WebSocketSyncClient::connect(std::string const& host, std::string const& port, std::vector<WebHeader> additionalHeaders)
{
    auto const results = resolver_.resolve(host, port);
    auto const ep = net::connect(ws_.next_layer(), results);

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    auto const hostPort = host + ':' + std::to_string(ep.port());

    ws_.set_option(boost::beast::websocket::stream_base::decorator([additionalHeaders = std::move(additionalHeaders
                                                                    )](boost::beast::websocket::request_type& req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
        for (auto const& header : additionalHeaders) {
            req.set(header.name, header.value);
        }
    }));

    ws_.handshake(hostPort, "/");
}

void
WebSocketSyncClient::disconnect()
{
    ws_.close(boost::beast::websocket::close_code::normal);
}

std::string
WebSocketSyncClient::syncPost(std::string const& body)
{
    boost::beast::flat_buffer buffer;

    ws_.write(net::buffer(std::string(body)));
    ws_.read(buffer);

    return boost::beast::buffers_to_string(buffer.data());
}

void
WebServerSslSyncClient::connect(std::string const& host, std::string const& port)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_none);

    tcp::resolver resolver{ioc_};
    ws_.emplace(ioc_, ctx);

    auto const results = resolver.resolve(host, port);
    net::connect(ws_->next_layer().next_layer(), results.begin(), results.end());
    ws_->next_layer().handshake(ssl::stream_base::client);

    ws_->set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

    ws_->handshake(host, "/");
}

void
WebServerSslSyncClient::disconnect()
{
    ws_->close(boost::beast::websocket::close_code::normal);
}

std::string
WebServerSslSyncClient::syncPost(std::string const& body)
{
    boost::beast::flat_buffer buffer;
    ws_->write(net::buffer(std::string(body)));
    ws_->read(buffer);

    return boost::beast::buffers_to_string(buffer.data());
}
