#pragma once

#include "util/config/ConfigDefinition.hpp"

#include <boost/asio/ssl/context.hpp>

#include <expected>
#include <optional>
#include <string>

namespace web::ng::impl {

std::expected<std::optional<boost::asio::ssl::context>, std::string>
makeServerSslContext(util::config::ClioConfigDefinition const& config);

std::expected<boost::asio::ssl::context, std::string>
makeServerSslContext(std::string const& certData, std::string const& keyData);

}  // namespace web::ng::impl
