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

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
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
#include <boost/beast/version.hpp>

#include <chrono>
#include <string>
#include <utility>

namespace util::requests {

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

RequestBuilder::RequestBuilder(std::string host, std::string port) : host_(std::move(host)), port_(std::move(port))
{
    request_.set(http::field::host, host);
}

RequestBuilder&
RequestBuilder::addHeader(boost::beast::http::field header, std::string value)
{
    request_.set(header, value);
    return *this;
}

RequestBuilder&
RequestBuilder::addData(std::string data)
{
    request_.body() = data;
    return *this;
}

RequestBuilder&
RequestBuilder::setTimeout(std::chrono::milliseconds const timeout)
{
    timeout_ = timeout;
    return *this;
}

Expected<std::string, RequestError>
RequestBuilder::get(boost::asio::yield_context yield)
{
    return requestImpl(yield, http::verb::get);
}

Expected<std::string, RequestError>
RequestBuilder::post(boost::asio::yield_context yield)
{
    return requestImpl(yield, http::verb::post);
}

Expected<std::string, RequestError>
RequestBuilder::requestImpl(boost::asio::yield_context yield, http::verb const method)
{
    auto executor = boost::asio::get_associated_executor(yield);

    // Look up the domain name
    beast::error_code errorCode;
    tcp::resolver resolver(executor);
    auto const resolverResult = resolver.async_resolve(host_, port_, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Resolve error", errorCode}};

    beast::tcp_stream stream(executor);

    // Make the connection on the IP address we get from a lookup
    stream.expires_after(timeout_);
    stream.async_connect(resolverResult, yield[errorCode]);
    if (errorCode)
        return Unexpected{RequestError{"Connection error", errorCode}};

    // Set up HTTP method
    request_.method(method);

    // Send the HTTP request to the remote host
    stream.expires_after(timeout_);
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

    // Gracefully close the socket
    stream.socket().shutdown(tcp::socket::shutdown_both, errorCode);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if (errorCode && errorCode != beast::errc::not_connected)
        return Unexpected{RequestError{"Shutdown socket error", errorCode}};

    if (response.result() != http::status::ok)
        return Unexpected{RequestError{"Response status not OK"}};

    return std::move(response).body();
}

}  // namespace util::requests
