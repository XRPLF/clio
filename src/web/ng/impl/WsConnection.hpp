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

#include "util/Taggable.hpp"
#include "util/WithTimeout.hpp"
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
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl.hpp>
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
    boost::beast::http::request<boost::beast::http::string_body> initialRequest_;

public:
    WsConnection(
        boost::asio::ip::tcp::socket socket,
        std::string ip,
        boost::beast::flat_buffer buffer,
        boost::beast::http::request<boost::beast::http::string_body> initialRequest,
        util::TagDecoratorFactory const& tagDecoratorFactory
    )
        requires IsTcpStream<StreamType>
        : Connection(std::move(ip), std::move(buffer), tagDecoratorFactory)
        , stream_(std::move(socket))
        , initialRequest_(std::move(initialRequest))
    {
    }

    WsConnection(
        boost::asio::ip::tcp::socket socket,
        std::string ip,
        boost::beast::flat_buffer buffer,
        boost::asio::ssl::context& sslContext,
        boost::beast::http::request<boost::beast::http::string_body> initialRequest,
        util::TagDecoratorFactory const& tagDecoratorFactory
    )
        requires IsSslTcpStream<StreamType>
        : Connection(std::move(ip), std::move(buffer), tagDecoratorFactory)
        , stream_(std::move(socket), sslContext)
        , initialRequest_(std::move(initialRequest))
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
    performHandshake(boost::asio::yield_context yield)
    {
        Error error;
        stream_.async_accept(initialRequest_, yield[error]);
        if (error)
            return error;
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
        auto error = util::withTimeout(
            [this, &response](auto&& yield) { stream_.async_write(response.asConstBuffer(), yield); }, yield, timeout
        );
        if (error)
            return std::move(error);
        return std::nullopt;
    }

    std::expected<Request, Error>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) override
    {
        auto error = util::withTimeout([this](auto&& yield) { stream_.async_read(buffer_, yield); }, yield, timeout);
        if (error)
            return std::unexpected{error};

        auto request = boost::beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        return Request{std::move(request), initialRequest_};
    }

    void
    close(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) override
    {
        boost::beast::websocket::stream_base::timeout wsTimeout{};
        stream_.get_option(wsTimeout);
        wsTimeout.handshake_timeout = timeout;
        stream_.set_option(wsTimeout);

        stream_.async_close(boost::beast::websocket::close_code::normal, yield);
    }
};

using PlainWsConnection = WsConnection<boost::beast::tcp_stream>;
using SslWsConnection = WsConnection<boost::asio::ssl::stream<boost::beast::tcp_stream>>;

std::expected<std::unique_ptr<PlainWsConnection>, Error>
make_PlainWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> request,
    util::TagDecoratorFactory const& tagDecoratorFactory,
    boost::asio::yield_context yield
);

std::expected<std::unique_ptr<SslWsConnection>, Error>
make_SslWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::ssl::context& sslContext,
    util::TagDecoratorFactory const& tagDecoratorFactory,
    boost::asio::yield_context yield
);

}  // namespace web::ng::impl
