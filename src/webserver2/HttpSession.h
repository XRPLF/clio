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

#include <webserver2/PlainWsSession.h>
#include <webserver2/details/HttpBase.h>

namespace Server {

using tcp = boost::asio::ip::tcp;

// Handles an HTTP server connection
template <ServerCallback Callback>
class HttpSession : public HttpBase<HttpSession, Callback>, public std::enable_shared_from_this<HttpSession<Callback>>
{
    boost::beast::tcp_stream stream_;

public:
    // Take ownership of the socket
    explicit HttpSession(
        tcp::socket&& socket,
        std::string const& ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Callback> const& callback,
        boost::beast::flat_buffer buffer)
        : HttpBase<HttpSession, Callback>(ip, tagFactory, dosGuard, callback, std::move(buffer))
        , stream_(std::move(socket))
    {
    }

    ~HttpSession() = default;

    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    boost::beast::tcp_stream
    releaseStream()
    {
        return std::move(stream_);
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this HttpSession. Although not strictly
        // necessary for single-threaded contexts, this example code is written
        // to be thread-safe by default.
        boost::asio::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(&HttpBase<HttpSession, Callback>::doRead, this->shared_from_this()));
    }

    void
    doClose()
    {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        // At this point the connection is closed gracefully
    }

    void
    upgrade()
    {
        std::make_shared<WsUpgrader<Callback>>(
            std::move(stream_),
            this->clientIp,
            this->tagFactory_,
            this->dosGuard_,
            this->callback_,
            std::move(this->buffer_),
            std::move(this->req_))
            ->run();
    }
};
}  // namespace Server
