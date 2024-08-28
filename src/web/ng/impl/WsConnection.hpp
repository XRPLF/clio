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

#include "util/build/Build.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/Concepts.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng::impl {

template <typename StreamType>
class WsConnection : public Connection {
    boost::beast::websocket::stream<StreamType> stream_;

public:
    WsConnection(boost::asio::ip::tcp::socket socket, std::string ip, boost::beast::flat_buffer buffer)
        requires IsTcpStream<StreamType>
        : Connection(std::move(ip), std::move(buffer)), stream_(std::move(socket))
    {
    }

    WsConnection(
        boost::asio::ip::tcp::socket socket,
        std::string ip,
        boost::beast::flat_buffer buffer,
        boost::asio::ssl::context& sslContext
    )
        requires IsSslTcpStream<StreamType>
        : Connection(std::move(ip), std::move(buffer)), stream_(std::move(socket), sslContext)
    {
        // Disable the timeout. The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(stream_).expires_never();
        stream_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
        stream_.set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
                res.set(boost::beast::http::field::server, util::build::getClioFullVersionString());
            })
        );
    }

    std::optional<Error>
    accept(
        boost::beast::http::request<boost::beast::http::string_body> const& request,
        boost::asio::yield_context yield
    )
    {
        boost::system::error_code error;
        stream_.async_accept(request, yield[error]);
        if (error)
            return Error{error};
        return std::nullopt;
    }

    bool
    wasUpgraded() const override
    {
        return true;
    }

    std::optional<Error>
    send(
        Response response,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT
    ) override
    {
        return {};
    }

    std::expected<Request, Error>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) override
    {
        return {};
    }

    void
    close(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) override
    {
    }
};

using PlainWsConnection = WsConnection<boost::beast::tcp_stream>;
using SslWsConnection = WsConnection<boost::asio::ssl::stream<boost::beast::tcp_stream>>;

std::expected<std::unique_ptr<PlainWsConnection>, Error>
make_PlainWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> const& request,
    boost::asio::yield_context yield
);

std::expected<std::unique_ptr<SslWsConnection>, Error>
make_SslWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> const& request,
    boost::asio::ssl::context& sslContext,
    boost::asio::yield_context yield
);

}  // namespace web::ng::impl
