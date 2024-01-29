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

#include "util/requests/RequestBuilder.h"

#include "util/Expected.h"
#include "util/requests/Types.h"
#include "util/requests/impl/StreamData.h"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <openssl/err.h>
#include <openssl/tls1.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace util::requests {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

RequestBuilder::RequestBuilder(std::string host, std::string port) : host_(std::move(host)), port_(std::move(port))
{
    request_.set(http::field::host, host_);
    request_.target("/");
}

RequestBuilder&
RequestBuilder::addHeader(HttpHeader const& header)
{
    request_.set(header.name, header.value);
    return *this;
}

RequestBuilder&
RequestBuilder::addHeaders(std::vector<HttpHeader> const& headers)
{
    for (auto const& header : headers)
        addHeader(header);
    return *this;
}

RequestBuilder&
RequestBuilder::addData(std::string data)
{
    request_.body() = data;
    request_.prepare_payload();
    return *this;
}

RequestBuilder&
RequestBuilder::setTimeout(std::chrono::milliseconds const timeout)
{
    timeout_ = timeout;
    return *this;
}

RequestBuilder&
RequestBuilder::setTarget(std::string_view target)
{
    request_.target(target);
    return *this;
}

RequestBuilder&
RequestBuilder::setSslEnabled(bool const enabled)
{
    sslEnabled_ = enabled;
    return *this;
}

Expected<std::string, RequestError>
RequestBuilder::get(asio::yield_context yield)
{
    return doRequest(yield, http::verb::get);
}

Expected<std::string, RequestError>
RequestBuilder::post(asio::yield_context yield)
{
    return doRequest(yield, http::verb::post);
}

Expected<std::string, RequestError>
RequestBuilder::doRequest(asio::yield_context yield, beast::http::verb method)
{
    if (sslEnabled_) {
        auto streamData = impl::SslTcpStreamData::create(yield);
        if (not streamData.has_value())
            return Unexpected{std::move(streamData.error())};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if (!SSL_set_tlsext_host_name(streamData->stream.native_handle(), host_.c_str())) {
#pragma GCC diagnostic pop
            beast::error_code errorCode;
            errorCode.assign(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category());
            return Unexpected{RequestError{"SSL setup failed", errorCode}};
        }
        return doRequestImpl(std::move(streamData.value()), yield, method);
    }

    auto streamData = impl::TcpStreamData{yield};
    return doRequestImpl(std::move(streamData), yield, method);
}

template <typename StreamDataType>
Expected<std::string, RequestError>
RequestBuilder::doRequestImpl(StreamDataType&& streamData, asio::yield_context yield, http::verb const method)
{
    auto executor = asio::get_associated_executor(yield);

    // Look up the domain name
    beast::error_code errorCode;
    tcp::resolver resolver(executor);
    auto const resolverResult = resolver.async_resolve(host_, port_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Resolve error", errorCode}};

    auto& stream = streamData.stream;

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(stream).expires_after(timeout_);
    beast::get_lowest_layer(stream).async_connect(resolverResult, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Connection error", errorCode}};

    // Set up HTTP method
    request_.method(method);

    // Perform SSL handshake
    if constexpr (StreamDataType::sslEnabled) {
        beast::get_lowest_layer(stream).expires_after(timeout_);
        stream.async_handshake(asio::ssl::stream_base::client, yield[errorCode]);
        if (errorCode)
            return Unexpected{RequestError{"Handshake error", errorCode}};
    }

    // Send the HTTP request to the remote host
    beast::get_lowest_layer(stream).expires_after(timeout_);
    http::async_write(stream, request_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Write error", errorCode}};

    // This buffer is used for reading and must be persisted
    beast::flat_buffer buffer;
    // Declare a container to hold the response
    http::response<http::string_body> response;

    // Receive the HTTP response
    http::async_read(stream, buffer, response, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Read error", errorCode}};

    if (response.result() != http::status::ok)
        return Unexpected{RequestError{"Response status not OK"}};

    // Gracefully close the socket
    beast::get_lowest_layer(stream).socket().shutdown(tcp::socket::shutdown_both, errorCode);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if (errorCode && errorCode != beast::errc::not_connected)
        return Unexpected{RequestError{"Shutdown socket error", errorCode}};

    return std::move(response).body();
}

}  // namespace util::requests
