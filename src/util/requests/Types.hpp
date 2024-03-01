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

#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>

#include <optional>
#include <string>
#include <variant>

namespace util::requests {

/**
 * @brief Error type for HTTP requests
 */
class RequestError {
    std::string message_;
    std::optional<boost::beast::error_code> errorCode_;

public:
    /**
     * @brief Construct a new Request Error object
     *
     * @param message error message
     */
    explicit RequestError(std::string message);

    /**
     * @brief Construct a new Request Error object
     *
     * @param message error message
     * @param errorCode error code from boost::beast
     */
    RequestError(std::string message, boost::beast::error_code errorCode);

    /**
     * @return The error message
     */
    std::string const&
    message() const;

    /**
     * @return The error code, if any
     */
    std::optional<boost::beast::error_code> const&
    errorCode() const;
};

/**
 * @brief HTTP header
 */
struct HttpHeader {
    /**
     * @brief Construct a new Http Header object
     *
     * @param name Header name
     * @param value Header value
     */
    HttpHeader(boost::beast::http::field name, std::string value);

    /**
     * @brief Construct a new Http Header object
     *
     * @param name Header name
     * @param value Header value
     */
    HttpHeader(std::string name, std::string value);

    std::variant<boost::beast::http::field, std::string> name; /**< Header name */
    std::string value;                                         /**< Header value */
};

}  // namespace util::requests
