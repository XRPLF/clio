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

    std::variant<boost::beast::http::field, std::string> name;
    std::string value;
};

}  // namespace util::requests
