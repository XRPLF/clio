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

#pragma once

#include <webserver/impl/WsBase.h>

namespace web {

/**
 * @brief The SSL WebSocket session class, just to hold the ssl stream. Other operations will be handled by the base
 * class.
 */
template <ServerHandler Handler>
class SslWsSession : public detail::WsBase<SslWsSession, Handler>
{
    using StreamType = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;
    StreamType ws_;

public:
    explicit SslWsSession(
        boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer&& b)
        : detail::WsBase<SslWsSession, Handler>(ip, tagFactory, dosGuard, handler, std::move(b)), ws_(std::move(stream))
    {
    }

    StreamType&
    ws()
    {
        return ws_;
    }
};

/**
 * @brief The SSL WebSocket upgrader class, upgrade from http session to websocket session.
 */
template <ServerHandler Handler>
class SslWsUpgrader : public std::enable_shared_from_this<SslWsUpgrader<Handler>>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> https_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::string ip_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::reference_wrapper<web::DOSGuard> dosGuard_;
    std::shared_ptr<Handler> const handler_;
    http::request<http::string_body> req_;

public:
    SslWsUpgrader(
        boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer&& buf,
        http::request<http::string_body> req)
        : https_(std::move(stream))
        , buffer_(std::move(buf))
        , ip_(ip)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , handler_(handler)
        , req_(std::move(req))
    {
    }

    ~SslWsUpgrader() = default;

    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(std::chrono::seconds(30));

        boost::asio::dispatch(
            https_.get_executor(),
            boost::beast::bind_front_handler(&SslWsUpgrader<Handler>::doUpgrade, this->shared_from_this()));
    }

private:
    void
    doUpgrade()
    {
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        constexpr static auto MaxBobySize = 10000;
        parser_->body_limit(MaxBobySize);

        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(std::chrono::seconds(30));

        onUpgrade();
    }

    void
    onUpgrade()
    {
        // See if it is a WebSocket Upgrade
        if (!boost::beast::websocket::is_upgrade(req_))
        {
            return;
        }

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(https_).expires_never();

        std::make_shared<SslWsSession<Handler>>(
            std::move(https_), ip_, this->tagFactory_, this->dosGuard_, this->handler_, std::move(buffer_))
            ->run(std::move(req_));
    }
};
}  // namespace web
