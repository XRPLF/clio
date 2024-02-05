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

#include "util/Expected.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace util::requests {

/**
 * @brief Builder for HTTP requests
 */
class RequestBuilder {
    util::Logger log_{"RequestBuilder"};
    std::string host_;
    std::string port_;
    std::chrono::milliseconds timeout_{DEFAULT_TIMEOUT};
    boost::beast::http::request<boost::beast::http::string_body> request_;

public:
    /**
     * @brief Construct a new Request Builder object
     *
     * @param host host to connect to
     * @param port port to connect to
     */
    RequestBuilder(std::string host, std::string port);

    /**
     * @brief Add a header to the request
     *
     * @param header header to add
     * @return reference to itself
     */
    RequestBuilder&
    addHeader(HttpHeader const& header);

    /**
     * @brief Add headers to the request
     *
     * @param headers headers to add
     * @return reference to itself
     */
    RequestBuilder&
    addHeaders(std::vector<HttpHeader> const& headers);

    /**
     * @brief Add body or data to the request
     *
     * @param data data to add
     * @return reference to itself
     */
    RequestBuilder&
    addData(std::string data);

    /**
     * @brief Set the timeout for the request
     *
     * @note Default timeout is defined in DEFAULT_TIMEOUT
     *
     * @param timeout timeout to set
     * @return reference to itself
     */
    RequestBuilder&
    setTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set the target for the request
     *
     * @note Default target is "/"
     *
     * @param target target to set
     * @return reference to itself
     */
    RequestBuilder&
    setTarget(std::string_view target);

    /**
     * @brief Perform a GET request with SSL asynchronously
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    getSsl(boost::asio::yield_context yield);

    /**
     * @brief Perform a GET request without SSL asynchronously
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    getPlain(boost::asio::yield_context yield);

    /**
     * @brief Perform a GET request asynchronously. The SSL will be used first, if it fails, the plain connection will
     * be used.
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    get(boost::asio::yield_context yield);

    /**
     * @brief Perform a POST request with SSL asynchronously
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    postSsl(boost::asio::yield_context yield);

    /**
     * @brief Perform a POST request without SSL asynchronously
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    postPlain(boost::asio::yield_context yield);

    /**
     * @brief Perform a POST request asynchronously. The SSL will be used first, if it fails, the plain connection will
     * be used.
     *
     * @note It is not thread-safe to call get() and post() of the same RequestBuilder from multiple threads. But it is
     * fine to call only get() or only post() of the same RequestBuilder from multiple threads.
     *
     * @param yield yield context
     * @return expected response or error
     */
    Expected<std::string, RequestError>
    post(boost::asio::yield_context yield);

    static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{30000};

private:
    Expected<std::string, RequestError>
    doSslRequest(boost::asio::yield_context yield, boost::beast::http::verb method);

    Expected<std::string, RequestError>
    doPlainRequest(boost::asio::yield_context yield, boost::beast::http::verb method);

    Expected<std::string, RequestError>
    doRequest(boost::asio::yield_context yield, boost::beast::http::verb method);

    template <typename StreamDataType>
    Expected<std::string, RequestError>
    doRequestImpl(StreamDataType&& streamData, boost::asio::yield_context yield, boost::beast::http::verb method);
};

}  // namespace util::requests
