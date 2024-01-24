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

#include "util/requests/WsConnection.h"

#include "util/Expected.h"
#include "util/requests/Types.h"

#include <boost/asio.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

#include <chrono>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace util::requests {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;

WsConnectionBuilder::WsConnectionBuilder(std::string host, std::string port)
    : host_(std::move(host)), port_(std::move(port))
{
}

WsConnectionBuilder&
WsConnectionBuilder::addHeader(HttpHeader header)
{
    headers_.push_back(std::move(header));
    return *this;
}

WsConnectionBuilder&
WsConnectionBuilder::addHeaders(std::vector<HttpHeader> headers)
{
    headers_.insert(headers_.end(), std::make_move_iterator(headers.begin()), std::make_move_iterator(headers.end()));
    return *this;
}

WsConnectionBuilder&
WsConnectionBuilder::setTarget(std::string target)
{
    target_ = std::move(target);
    return *this;
}

WsConnectionBuilder&
WsConnectionBuilder::setTimeout(std::chrono::milliseconds timeout)
{
    timeout_ = timeout;
    return *this;
}

WsConnectionBuilder&
WsConnectionBuilder::setSslEnabled(bool sslEnabled)
{
    sslEnabled_ = sslEnabled;
    return *this;
}

Expected<WsConnection, RequestError>
WsConnectionBuilder::connect(asio::yield_context yield) const
{
    auto context = asio::get_associated_executor(yield);
    beast::error_code errorCode;

    // These objects perform our I/O
    asio::ip::tcp::resolver resolver(context);
    websocket::stream<beast::tcp_stream> ws(context);

    // Look up the domain name
    auto const results = resolver.async_resolve(host_, port_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Resolve error", errorCode}};

    // Set a timeout on the operation
    beast::get_lowest_layer(ws).expires_after(timeout_);

    // Make the connection on the IP address we get from a lookup
    auto endpoint = beast::get_lowest_layer(ws).async_connect(results, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Connect error", errorCode}};

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(ws).expires_never();

    // Set suggested timeout settings for the websocket
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    ws.set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
        for (auto const& header : headers_)
            req.set(header.name, header.value);
    }));

    // Perform the websocket handshake
    std::string const host = fmt::format("{}:{}", host_, endpoint.port());
    ws.async_handshake(host, target_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Handshake error", errorCode}};

    return WsConnection{std::move(ws)};
}

WsConnection::WsConnection(websocket::stream<beast::tcp_stream> ws) : ws_(std::move(ws))
{
}

Expected<std::string, RequestError>
WsConnection::read(asio::yield_context yield)
{
    beast::error_code errorCode;
    beast::flat_buffer buffer;

    ws_.async_read(buffer, yield[errorCode]);

    if (errorCode)
        return Unexpected{RequestError{"Read error", errorCode}};

    return beast::buffers_to_string(std::move(buffer).data());
}

std::optional<RequestError>
WsConnection::write(std::string const& message, asio::yield_context yield)
{
    beast::error_code errorCode;
    ws_.async_write(asio::buffer(message), yield[errorCode]);

    if (errorCode)
        return RequestError{"Write error", errorCode};

    return std::nullopt;
}

std::optional<RequestError>
WsConnection::close(asio::yield_context yield)
{
    beast::error_code errorCode;
    ws_.async_close(websocket::close_code::normal, yield[errorCode]);
    if (errorCode)
        return RequestError{"Close error", errorCode};
    return std::nullopt;
}

}  // namespace util::requests
