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

#include "util/TestHttpClient.hpp"

#include "util/Assert.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <openssl/err.h>
#include <openssl/tls1.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace {

std::string
syncRequest(
    std::string const& host,
    std::string const& port,
    std::string const& body,
    std::vector<WebHeader> additionalHeaders,
    http::verb method,
    std::string target = "/"
)
{
    boost::asio::io_context ioc;

    net::ip::tcp::resolver resolver(ioc);
    boost::beast::tcp_stream stream(ioc);

    auto const results = resolver.resolve(host, port);
    stream.connect(results);

    http::request<http::string_body> req{method, "/", 10};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for (auto& header : additionalHeaders) {
        req.set(header.name, header.value);
    }

    req.target(target);
    req.body() = std::string(body);
    req.prepare_payload();
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    boost::beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    return res.body();
}

}  // namespace

WebHeader::WebHeader(http::field name, std::string value) : name(name), value(std::move(value))
{
}

std::string
HttpSyncClient::post(
    std::string const& host,
    std::string const& port,
    std::string const& body,
    std::vector<WebHeader> additionalHeaders
)
{
    return syncRequest(host, port, body, std::move(additionalHeaders), http::verb::post);
}

std::string
HttpSyncClient::get(
    std::string const& host,
    std::string const& port,
    std::string const& body,
    std::string const& target,
    std::vector<WebHeader> additionalHeaders
)
{
    return syncRequest(host, port, body, std::move(additionalHeaders), http::verb::get, target);
}

bool
HttpsSyncClient::verify_certificate(bool /* preverified */, boost::asio::ssl::verify_context& /* ctx */)
{
    return true;
}

std::string
HttpsSyncClient::syncPost(std::string const& host, std::string const& port, std::string const& body)
{
    net::io_context ioc;
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_none);

    tcp::resolver resolver(ioc);
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ctx);

// We can't fix this so have to ignore
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
#pragma GCC diagnostic pop
    {
        boost::beast::error_code const ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        throw boost::beast::system_error{ec};
    }

    auto const results = resolver.resolve(host, port);
    boost::beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::post, "/", 10};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.body() = std::string(body);
    req.prepare_payload();
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    boost::beast::error_code ec;
    stream.shutdown(ec);

    return res.body();
}

HttpAsyncClient::HttpAsyncClient(boost::asio::io_context& ioContext) : stream_{ioContext}
{
}

std::optional<boost::system::error_code>
HttpAsyncClient::connect(
    std::string_view host,
    std::string_view port,
    boost::asio::yield_context yield,
    std::chrono::steady_clock::duration timeout
)
{
    boost::system::error_code error;
    boost::asio::ip::tcp::resolver resolver{stream_.get_executor()};
    auto const resolverResults = resolver.resolve(host, port, error);
    if (error)
        return error;

    ASSERT(resolverResults.size() > 0, "No results from resolver");

    boost::beast::get_lowest_layer(stream_).expires_after(timeout);
    stream_.async_connect(resolverResults.begin()->endpoint(), yield[error]);
    if (error)
        return error;
    return std::nullopt;
}

std::optional<boost::system::error_code>
HttpAsyncClient::send(
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::yield_context yield,
    std::chrono::steady_clock::duration timeout
)
{
    request.prepare_payload();
    boost::system::error_code error;
    boost::beast::get_lowest_layer(stream_).expires_after(timeout);
    http::async_write(stream_, request, yield[error]);
    if (error)
        return error;
    return std::nullopt;
}

std::expected<boost::beast::http::response<boost::beast::http::string_body>, boost::system::error_code>
HttpAsyncClient::receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout)
{
    boost::system::error_code error;
    http::response<http::string_body> response;
    boost::beast::get_lowest_layer(stream_).expires_after(timeout);
    http::async_read(stream_, buffer_, response, yield[error]);
    if (error)
        return std::unexpected{error};
    return response;
}

void
HttpAsyncClient::gracefulShutdown()
{
    boost::system::error_code error;
    stream_.socket().shutdown(tcp::socket::shutdown_both, error);
}

void
HttpAsyncClient::disconnect()
{
    boost::system::error_code error;
    stream_.socket().close(error);
}
