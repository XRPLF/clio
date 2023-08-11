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

#include <web/PlainWsSession.h>
#include <web/impl/HttpBase.h>

namespace web {

using tcp = boost::asio::ip::tcp;

/**
 * @brief The HTTP session class
 * It will handle the upgrade to WebSocket, pass the ownership of the socket to the upgrade session.
 * Otherwise, it will pass control to the base class.
 */
template <ServerHandler Handler>
class HttpSession : public detail::HttpBase<HttpSession, Handler>,
                    public std::enable_shared_from_this<HttpSession<Handler>>
{
    boost::beast::tcp_stream stream_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;

public:
    explicit HttpSession(
        tcp::socket&& socket,
        std::string const& ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer buffer)
        : detail::HttpBase<HttpSession, Handler>(ip, tagFactory, dosGuard, handler, std::move(buffer))
        , stream_(std::move(socket))
        , tagFactory_(tagFactory)
    {
    }

    ~HttpSession() = default;

    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    void
    run()
    {
        boost::asio::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &detail::HttpBase<HttpSession, Handler>::doRead, this->shared_from_this()));
    }

    void
    doClose()
    {
        boost::beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    void
    upgrade()
    {
        std::make_shared<WsUpgrader<Handler>>(
            std::move(stream_),
            this->clientIp,
            tagFactory_,
            this->dosGuard_,
            this->handler_,
            std::move(this->buffer_),
            std::move(this->req_))
            ->run();
    }
};
}  // namespace web
