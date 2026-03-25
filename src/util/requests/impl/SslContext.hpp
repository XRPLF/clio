#pragma once

#include "util/requests/Types.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/error.hpp>

#include <expected>
#include <optional>
#include <string>

namespace util::requests::impl {

std::expected<boost::asio::ssl::context, RequestError>
makeClientSslContext();

std::optional<std::string>
sslErrorToString(boost::beast::error_code const& error);

}  // namespace util::requests::impl
