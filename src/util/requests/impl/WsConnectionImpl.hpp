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
#include "util/requests/WsConnection.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <optional>
#include <string>
#include <utility>

namespace util::requests::impl {

template <typename StreamType>
class WsConnectionImpl : public WsConnection {
    StreamType ws_;

public:
    explicit WsConnectionImpl(StreamType ws) : ws_(std::move(ws))
    {
    }

    Expected<std::string, RequestError>
    read(boost::asio::yield_context yield) override
    {
        boost::beast::error_code errorCode;
        boost::beast::flat_buffer buffer;

        ws_.async_read(buffer, yield[errorCode]);

        if (errorCode)
            return Unexpected{RequestError{"Read error", errorCode}};

        return boost::beast::buffers_to_string(std::move(buffer).data());
    }

    std::optional<RequestError>
    write(std::string const& message, boost::asio::yield_context yield) override
    {
        boost::beast::error_code errorCode;
        ws_.async_write(boost::asio::buffer(message), yield[errorCode]);

        if (errorCode)
            return RequestError{"Write error", errorCode};

        return std::nullopt;
    }

    std::optional<RequestError>
    close(boost::asio::yield_context yield) override
    {
        boost::beast::error_code errorCode;
        ws_.async_close(boost::beast::websocket::close_code::normal, yield[errorCode]);
        if (errorCode)
            return RequestError{"Close error", errorCode};
        return std::nullopt;
    }
};

using PlainWsConnection = WsConnectionImpl<boost::beast::websocket::stream<boost::beast::tcp_stream>>;
using SslWsConnection =
    WsConnectionImpl<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>;

}  // namespace util::requests::impl
