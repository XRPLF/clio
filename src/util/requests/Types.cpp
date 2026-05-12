#include "util/requests/Types.hpp"

#include "util/requests/impl/SslContext.hpp"

#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>

#include <optional>
#include <string>
#include <utility>

namespace util::requests {

RequestError::RequestError(std::string message) : message_(std::move(message))
{
}

RequestError::RequestError(std::string message, boost::beast::error_code errorCode)
    : message_(std::move(message)), errorCode_(errorCode)
{
    message_.append(": ");
    if (auto const sslError = impl::sslErrorToString(errorCode); sslError.has_value()) {
        message_.append(*sslError);
    } else {
        message_.append(errorCode.message());
    }
}

std::string const&
RequestError::message() const
{
    return message_;
}

std::optional<boost::beast::error_code> const&
RequestError::errorCode() const
{
    return errorCode_;
}

HttpHeader::HttpHeader(boost::beast::http::field name, std::string value)
    : name(name), value(std::move(value))
{
}

HttpHeader::HttpHeader(std::string name, std::string value)
    : name(std::move(name)), value(std::move(value))
{
}

}  // namespace util::requests
