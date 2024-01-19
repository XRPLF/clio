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

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

namespace util::requests::impl {

struct TcpStreamData {
    explicit TcpStreamData(boost::asio::yield_context yield);

    // Does nothing for plain TCP
    static boost::beast::error_code doHandshake(boost::asio::yield_context);

    boost::beast::tcp_stream stream;
};

class SslTcpStreamData {
    boost::asio::ssl::context sslContext_;

public:
    static Expected<SslTcpStreamData, RequestError>
    create(boost::asio::yield_context yield);

    boost::beast::ssl_stream<boost::beast::tcp_stream> stream;

    boost::beast::error_code
    doHandshake(boost::asio::yield_context yield);

private:
    explicit SslTcpStreamData(boost::asio::ssl::context context, boost::asio::yield_context yield);
};

}  // namespace util::requests::impl
