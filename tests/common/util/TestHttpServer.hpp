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

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <functional>
#include <optional>
#include <string>

/**
 * @brief Simple HTTP server for use in unit tests
 */
class TestHttpServer {
public:
    using RequestHandler = std::function<std::optional<boost::beast::http::response<
        boost::beast::http::string_body>>(boost::beast::http::request<boost::beast::http::string_body>)>;

    /**
     * @brief Construct a new TestHttpServer
     *
     * @param context boost::asio::io_context to use for networking
     * @param host host to bind to
     */
    TestHttpServer(boost::asio::io_context& context, std::string host);

    /**
     * @brief Start the server
     *
     * @note This method schedules to process only one request
     *
     * @param handler RequestHandler to use for incoming request
     * @param allowToFail if true, the server will not throw an exception if the request fails
     */
    void
    handleRequest(RequestHandler handler, bool allowToFail = false);

    /**
     * @brief Return the port HTTP server is connected to
     *
     * @return string port number
     */
    std::string
    port() const;

private:
    boost::asio::ip::tcp::acceptor acceptor_;
};
