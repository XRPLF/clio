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

#include "util/requests/impl/StreamData.h"

#include "util/Expected.h"
#include "util/requests/Types.h"
#include "util/requests/impl/SslContext.h"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/error.hpp>

#include <utility>

namespace util::requests::impl {

namespace asio = boost::asio;
namespace ssl = asio::ssl;

TcpStreamData::TcpStreamData(asio::yield_context yield) : stream(asio::get_associated_executor(yield))
{
}

SslTcpStreamData::SslTcpStreamData(ssl::context sslContext, boost::asio::yield_context yield)
    : sslContext_(std::move(sslContext)), stream(asio::get_associated_executor(yield), sslContext_)
{
}

Expected<SslTcpStreamData, RequestError>
SslTcpStreamData::create(boost::asio::yield_context yield)
{
    auto sslContext = makeSslContext();
    if (not sslContext.has_value()) {
        return Unexpected{std::move(sslContext.error())};
    }
    return SslTcpStreamData{std::move(sslContext.value()), yield};
}

WsStreamData::WsStreamData(boost::asio::yield_context yield) : stream(asio::get_associated_executor(yield))
{
}

SslWsStreamData::SslWsStreamData(ssl::context sslContext, boost::asio::yield_context yield)
    : sslContext_(std::move(sslContext)), stream(asio::get_associated_executor(yield), sslContext_)
{
}

Expected<SslWsStreamData, RequestError>
SslWsStreamData::create(boost::asio::yield_context yield)
{
    auto sslContext = makeSslContext();
    if (not sslContext.has_value()) {
        return Unexpected{std::move(sslContext.error())};
    }
    return SslWsStreamData{std::move(sslContext.value()), yield};
}

}  // namespace util::requests::impl
