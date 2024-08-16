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

#include "web/ng/Connection.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/basic_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <chrono>
#include <utility>

namespace web::ng::impl {

template <typename StreamType>
class HttpConnection : public Connection {
    StreamType stream_;

public:
    HttpConnection(boost::asio::ip::tcp::socket socket)
        requires std::is_same_v<StreamType, boost::beast::tcp_stream>
        : stream_{std::move(socket)}
    {
    }

    HttpConnection(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context& sslCtx)
        requires std::is_same_v<StreamType, boost::asio::ssl::stream<boost::beast::tcp_stream>>
        : stream_{std::move(socket), sslCtx}
    {
    }

    void
    send() override
    {
    }

    void
    receive() override
    {
    }
    void
    close(std::chrono::steady_clock::duration) override
    {
    }
};

using PlainHttpConnection = HttpConnection<boost::beast::tcp_stream>;

using SslHttpConnection = HttpConnection<boost::asio::ssl::stream<boost::beast::tcp_stream>>;

}  // namespace web::ng::impl
