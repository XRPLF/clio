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

#include "web/ng/impl/WsConnection.hpp"

#include "util/Taggable.hpp"
#include "web/ng/Error.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <memory>
#include <string>
#include <utility>

namespace web::ng::impl {

std::expected<std::unique_ptr<PlainWsConnection>, Error>
make_PlainWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> request,
    util::TagDecoratorFactory const& tagDecoratorFactory,
    boost::asio::yield_context yield
)
{
    auto connection = std::make_unique<PlainWsConnection>(
        std::move(socket), std::move(ip), std::move(buffer), std::move(request), tagDecoratorFactory
    );
    auto maybeError = connection->performHandshake(yield);
    if (maybeError.has_value())
        return std::unexpected{maybeError.value()};
    return connection;
}

std::expected<std::unique_ptr<SslWsConnection>, Error>
make_SslWsConnection(
    boost::asio::ip::tcp::socket socket,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::ssl::context& sslContext,
    util::TagDecoratorFactory const& tagDecoratorFactory,
    boost::asio::yield_context yield
)
{
    auto connection = std::make_unique<SslWsConnection>(
        std::move(socket), std::move(ip), std::move(buffer), sslContext, std::move(request), tagDecoratorFactory
    );
    auto maybeError = connection->performHandshake(yield);
    if (maybeError.has_value())
        return std::unexpected{maybeError.value()};
    return connection;
}

}  // namespace web::ng::impl
