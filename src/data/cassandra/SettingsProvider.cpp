//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "data/cassandra/SettingsProvider.hpp"

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Cluster.hpp"
#include "util/Constants.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace data::cassandra {

namespace impl {

inline Settings::SecureConnectionBundle
invoke_tag_SecureConnections(std::string_view value)
{
    return Settings::SecureConnectionBundle{value.data()};
}
}  // namespace impl

SettingsProvider::SettingsProvider(util::config::ObjectView const& cfg)
    : config_{cfg}
    , keyspace_{cfg.getValue("keyspace").asString()}
    , tablePrefix_{cfg.getValue("table_prefix").hasValue() ? std::make_optional(cfg.getValue("table_prefix").asString()) : std::nullopt}
    , replicationFactor_{cfg.getValue("replication_factor").asIntType<uint16_t>()}
    , settings_{parseSettings()}
{
}

Settings
SettingsProvider::getSettings() const
{
    return settings_;
}

std::optional<std::string>
SettingsProvider::parseOptionalCertificate() const
{
    if (auto const certPath = config_.getValue("certfile"); certPath.hasValue()) {
        auto const path = std::filesystem::path(certPath.asString());
        std::ifstream fileStream(path.string(), std::ios::in);
        if (!fileStream) {
            throw std::system_error(errno, std::generic_category(), "Opening certificate " + path.string());
        }

        std::string contents(std::istreambuf_iterator<char>{fileStream}, std::istreambuf_iterator<char>{});
        if (fileStream.bad()) {
            throw std::system_error(errno, std::generic_category(), "Reading certificate " + path.string());
        }

        return contents;
    }

    return std::nullopt;
}

Settings
SettingsProvider::parseSettings() const
{
    auto settings = Settings::defaultSettings();

    // all config values used in settings is under "database.cassandra" prefix
    if (config_.getValue("secure_connect_bundle").hasValue()) {
        auto const bundle = impl::invoke_tag_SecureConnections(config_.getValue("secure_connect_bundle").asString());
        settings.connectionInfo = bundle;
    } else {
        Settings::ContactPoints out;
        out.contactPoints = config_.getValue("contact_points").asString();
        out.port = config_.getValue("port").hasValue()
            ? std::make_optional(config_.getValue("port").asIntType<uint32_t>())
            : std::nullopt;
        settings.connectionInfo = out;
    }

    settings.threads = config_.getValue("threads").asIntType<uint32_t>();
    settings.maxWriteRequestsOutstanding = config_.getValue("max_write_requests_outstanding").asIntType<uint32_t>();
    settings.maxReadRequestsOutstanding = config_.getValue("max_read_requests_outstanding").asIntType<uint32_t>();
    settings.coreConnectionsPerHost = config_.getValue("core_connections_per_host").asIntType<uint32_t>();
    settings.queueSizeIO = config_.getValue("queue_size_io").hasValue()
        ? std::make_optional(config_.getValue("queue_size_io").asIntType<uint32_t>())
        : std::nullopt;
    settings.writeBatchSize = config_.getValue("write_batch_size").asIntType<std::size_t>();

    if (config_.getValue("connect_timeout").hasValue()) {
        auto const connectTimeoutSecond = config_.getValue("connect_timeout").asIntType<uint32_t>();
        settings.connectionTimeout = std::chrono::milliseconds{connectTimeoutSecond * util::MILLISECONDS_PER_SECOND};
    }

    if (config_.getValue("request_timeout").hasValue()) {
        auto const requestTimeoutSecond = config_.getValue("request_timeout").asIntType<uint32_t>();
        settings.requestTimeout = std::chrono::milliseconds{requestTimeoutSecond * util::MILLISECONDS_PER_SECOND};
    }

    auto const username = config_.getValue("username");
    auto const password = config_.getValue("password");
    settings.certificate = parseOptionalCertificate();
    settings.username = username.hasValue() ? std::make_optional(username.asString()) : std::nullopt;
    settings.password = password.hasValue() ? std::make_optional(password.asString()) : std::nullopt;

    return settings;
}

}  // namespace data::cassandra
