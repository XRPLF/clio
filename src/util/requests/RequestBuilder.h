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

#include <boost/asio/spawn.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

#include <chrono>
#include <string>
#include <utility>

namespace util::requests {

struct RequestError {
    explicit RequestError(std::string message) : message(std::move(message))
    {
    }

    RequestError(std::string message, boost::beast::error_code ec) : message(std::move(message))
    {
        message.append(": ");
        message.append(ec.message());
    }

    std::string message;
};

class RequestBuilder {
    std::string host_;
    std::string port_;
    std::chrono::milliseconds timeout_{DEFAULT_TIMEOUT};
    boost::beast::http::request<boost::beast::http::string_body> request_;

public:
    RequestBuilder(std::string host, std::string port);

    RequestBuilder&
    addHeader(boost::beast::http::field header, std::string value);

    RequestBuilder&
    addData(std::string data);

    // default timeout is 30s
    RequestBuilder&
    setTimeout(std::chrono::milliseconds timeout);

    // default target is "/"
    RequestBuilder&
    setTarget(std::string target);

    Expected<std::string, RequestError>
    get(boost::asio::yield_context yield);

    Expected<std::string, RequestError>
    post(boost::asio::yield_context yield);

    static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{30000};

private:
    Expected<std::string, RequestError>
    requestImpl(boost::asio::yield_context yield, boost::beast::http::verb method);
};

}  // namespace util::requests
