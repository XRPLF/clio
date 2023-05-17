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

#include <webserver2/details/WsBase.h>

namespace Server {
// Echoes back all received WebSocket messages
template <ServerCallback Callback>
class PlainWsSession : public WsSession<PlainWsSession, Callback>
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;

public:
    // Take ownership of the socket
    explicit PlainWsSession(
        boost::asio::ip::tcp::socket&& socket,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Callback> const& callback,
        boost::beast::flat_buffer&& buffer)
        : WsSession<PlainWsSession, Callback>(ip, tagFactory, dosGuard, callback, std::move(buffer))
        , ws_(std::move(socket))
    {
    }

    boost::beast::websocket::stream<boost::beast::tcp_stream>&
    ws()
    {
        return ws_;
    }

    ~PlainWsSession() = default;
};

template <ServerCallback Callback>
class WsUpgrader : public std::enable_shared_from_this<WsUpgrader<Callback>>
{
    boost::beast::tcp_stream http_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::reference_wrapper<clio::DOSGuard> dosGuard_;
    http::request<http::string_body> req_;
    std::string ip_;
    std::shared_ptr<Callback> const callback_;

public:
    WsUpgrader(
        boost::beast::tcp_stream&& stream,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Callback> const& callback,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : http_(std::move(stream))
        , buffer_(std::move(b))
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , req_(std::move(req))
        , ip_(ip)
        , callback_(callback)
    {
    }

    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.

        boost::asio::dispatch(
            http_.get_executor(),
            boost::beast::bind_front_handler(&WsUpgrader<Callback>::doUpgrade, this->shared_from_this()));
    }

private:
    void
    doUpgrade()
    {
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(http_).expires_after(std::chrono::seconds(30));

        onUpgrade();
    }

    void
    onUpgrade()
    {
        // See if it is a WebSocket Upgrade
        if (!boost::beast::websocket::is_upgrade(req_))
            return;

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(http_).expires_never();

        std::make_shared<PlainWsSession<Callback>>(
            http_.release_socket(), ip_, tagFactory_, dosGuard_, callback_, std::move(buffer_))
            ->run(std::move(req_));
    }
};

}  // namespace Server
