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

#include "util/requests/WsConnection.hpp"

#include "util/Expected.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/impl/StreamData.hpp"
#include "util/requests/impl/WsConnectionImpl.hpp"

#include <boost/asio.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
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
#include <openssl/err.h>
#include <openssl/tls1.h>

#include <chrono>
#include <iterator>
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
WsConnectionBuilder::setConnectionTimeout(std::chrono::milliseconds timeout)
{
    timeout_ = timeout;
    return *this;
}

Expected<WsConnectionPtr, RequestError>
WsConnectionBuilder::sslConnect(asio::yield_context yield) const
{
    auto streamData = impl::SslWsStreamData::create(yield);
    if (not streamData.has_value())
        return Unexpected{std::move(streamData).error()};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    if (!SSL_set_tlsext_host_name(streamData->stream.next_layer().native_handle(), host_.c_str())) {
#pragma GCC diagnostic pop
        beast::error_code errorCode;
        errorCode.assign(static_cast<int>(::ERR_get_error()), beast::net::error::get_ssl_category());
        return Unexpected{RequestError{"SSL setup failed", errorCode}};
    }
    return connectImpl(std::move(streamData).value(), yield);
}

Expected<WsConnectionPtr, RequestError>
WsConnectionBuilder::plainConnect(asio::yield_context yield) const
{
    return connectImpl(impl::WsStreamData{yield}, yield);
}

Expected<WsConnectionPtr, RequestError>
WsConnectionBuilder::connect(asio::yield_context yield) const
{
    auto sslConnection = sslConnect(yield);
    if (sslConnection.has_value())
        return sslConnection;
    LOG(log_.debug()) << "SSL connection failed with error: " << sslConnection.error().message
                      << ". Falling back to plain connection.";

    return plainConnect(yield);
}

template <typename StreamDataType>
Expected<WsConnectionPtr, RequestError>
WsConnectionBuilder::connectImpl(StreamDataType&& streamData, asio::yield_context yield) const
{
    auto context = asio::get_associated_executor(yield);
    beast::error_code errorCode;

    asio::ip::tcp::resolver resolver(context);
    auto const results = resolver.async_resolve(host_, port_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Resolve error", errorCode}};

    auto& ws = streamData.stream;

    beast::get_lowest_layer(ws).expires_after(timeout_);
    auto endpoint = beast::get_lowest_layer(ws).async_connect(results, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Connect error", errorCode}};

    if constexpr (StreamDataType::sslEnabled) {
        beast::get_lowest_layer(ws).expires_after(timeout_);
        ws.next_layer().async_handshake(asio::ssl::stream_base::client, yield[errorCode]);
        if (errorCode)
            return Unexpected{RequestError{"SSL handshake error", errorCode}};
    }

    // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system
    beast::get_lowest_layer(ws).expires_never();

    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws.set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
        for (auto const& header : headers_)
            std::visit([&](auto const& name) { req.set(name, header.value); }, header.name);
    }));

    std::string const host = fmt::format("{}:{}", host_, endpoint.port());
    ws.async_handshake(host, target_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Handshake error", errorCode}};

    if constexpr (StreamDataType::sslEnabled) {
        return std::make_unique<impl::SslWsConnection>(std::move(ws));
    } else {
        return std::make_unique<impl::PlainWsConnection>(std::move(ws));
    }
}

}  // namespace util::requests
