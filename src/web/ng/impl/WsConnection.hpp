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

#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <type_traits>

namespace web::ng::impl {

template <typename T>
concept IsWsStream = std::is_same_v<T, boost::beast::websocket::stream<boost::beast::tcp_stream>>;

template <typename T>
concept IsWsSslStream =
    std::is_same_v<T, boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>>;

template <typename StreamType>
class WsConnection : public Connection {
    StreamType stream_;

public:
    WsConnection(boost::asio::ip::tcp::socket socket, std::string ip, boost::beast::flat_buffer buffer, boost::beast::
};

using PlainWsConnection = WsConnection<boost::beast::websocket::stream<boost::beast::tcp_stream>>;
using SslWsConnection =
    WsConnection<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>>;

}  // namespace web::ng::impl
