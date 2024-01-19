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

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace util::requests {

class WsConnection;

/**
 * @brief Builder for WebSocket connections
 */
class WsConnectionBuilder {
    std::string host_;
    std::string port_;
    std::vector<HttpHeader> headers_;
    std::chrono::milliseconds timeout_{DEFAULT_TIMEOUT};
    std::string target_{"/"};

public:
    WsConnectionBuilder(std::string host, std::string port);

    /**
     * @brief Add a header to the request
     *
     * @param header header to add
     * @return RequestBuilder& this
     */
    WsConnectionBuilder&
    addHeader(HttpHeader header);

    /**
     * @brief Add multiple headers to the request
     *
     * @param headers headers to add
     * @return RequestBuilder& this
     */
    WsConnectionBuilder&
    addHeaders(std::vector<HttpHeader> headers);

    /**
     * @brief Set the target of the request
     *
     * @param target target to set
     * @return RequestBuilder& this
     */
    WsConnectionBuilder&
    setTarget(std::string target);

    /**
     * @brief Set the timeout of the request
     *
     * @param timeout timeout to set
     * @return RequestBuilder& this
     */
    WsConnectionBuilder&
    setTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Connect to the host asynchronously
     *
     * @param yield yield context
     * @return Expected<WsConnection, RequestError> WebSocket connection or error
     */
    Expected<WsConnection, RequestError>
    connect(boost::asio::yield_context yield) const;

    static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{5000};
};

class WsConnection {
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;

public:
    Expected<std::string, RequestError>
    read(boost::asio::yield_context yield);

    std::optional<RequestError>
    write(std::string const& message, boost::asio::yield_context yield);

    std::optional<RequestError>
    close(boost::asio::yield_context yield);

private:
    explicit WsConnection(boost::beast::websocket::stream<boost::beast::tcp_stream> ws);

    friend WsConnectionBuilder;
};

}  // namespace util::requests
