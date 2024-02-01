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

#include "util/Expected.h"
#include "util/requests/Types.h"
#include "util/requests/impl/SslContext.h"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <utility>

namespace util::requests::impl {

template <typename StreamType>
struct PlainStreamData {
    static constexpr bool sslEnabled = false;

    explicit PlainStreamData(boost::asio::yield_context yield) : stream(boost::asio::get_associated_executor(yield))
    {
    }

    StreamType stream;
};

using TcpStreamData = PlainStreamData<boost::beast::tcp_stream>;
using WsStreamData = PlainStreamData<boost::beast::websocket::stream<boost::beast::tcp_stream>>;

template <typename StreamType>
class SslStreamData {
    boost::asio::ssl::context sslContext_;

public:
    static constexpr bool sslEnabled = true;

    static Expected<SslStreamData, RequestError>
    create(boost::asio::yield_context yield)
    {
        auto sslContext = makeSslContext();
        if (not sslContext.has_value()) {
            return Unexpected{std::move(sslContext.error())};
        }
        return SslStreamData{std::move(sslContext).value(), yield};
    }

    StreamType stream;

private:
    SslStreamData(boost::asio::ssl::context sslContext, boost::asio::yield_context yield)
        : sslContext_(std::move(sslContext)), stream(boost::asio::get_associated_executor(yield), sslContext_)

    {
    }
};

using SslTcpStreamData = SslStreamData<boost::beast::ssl_stream<boost::beast::tcp_stream>>;
using SslWsStreamData =
    SslStreamData<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>;

}  // namespace util::requests::impl
