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

#include "util/requests/impl/TcpStreamData.h"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>

namespace util::requests::impl {

namespace asio = boost::asio;
namespace ssl = asio::ssl;

namespace {

ssl::context
makeSslContext()
{
    ssl::context context{ssl::context::tlsv12_client};
    context.set_default_verify_paths();
    context.set_verify_mode(ssl::verify_peer);
    return context;
}

}  // namespace

TcpStreamData::TcpStreamData(asio::yield_context& yield) : stream(asio::get_associated_executor(yield))
{
}

SslTcpStreamData::SslTcpStreamData(boost::asio::yield_context& yield)
    : sslContext_(makeSslContext()), stream(asio::get_associated_executor(yield), sslContext_)
{
}

}  // namespace util::requests::impl
