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
    , keyspace_{cfg.getValue<std::string>("keyspace")}
    , tablePrefix_{cfg.maybeValue<std::string>("table_prefix")}
    , replicationFactor_{cfg.getValue<uint16_t>("replication_factor")}
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
    if (auto const certPath = config_.getValueView("certfile"); certPath.hasValue()) {
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
    if (config_.getValueView("secure_connect_bundle").hasValue()) {
        auto const bundle = Settings::SecureConnectionBundle{(config_.getValue<std::string>("secure_connect_bundle"))};
        settings.connectionInfo = bundle;
    } else {
        Settings::ContactPoints out;
        out.contactPoints = config_.getValue<std::string>("contact_points");
        out.port = config_.maybeValue<uint32_t>("port");
        settings.connectionInfo = out;
    }

    settings.threads = config_.getValue<uint32_t>("threads");
    settings.maxWriteRequestsOutstanding = config_.getValue<uint32_t>("max_write_requests_outstanding");
    settings.maxReadRequestsOutstanding = config_.getValue<uint32_t>("max_read_requests_outstanding");
    settings.coreConnectionsPerHost = config_.getValue<uint32_t>("core_connections_per_host");
    settings.queueSizeIO = config_.maybeValue<uint32_t>("queue_size_io");
    settings.writeBatchSize = config_.getValue<std::size_t>("write_batch_size");

    if (config_.getValueView("connect_timeout").hasValue()) {
        auto const connectTimeoutSecond = config_.getValue<uint32_t>("connect_timeout");
        settings.connectionTimeout = std::chrono::milliseconds{connectTimeoutSecond * util::MILLISECONDS_PER_SECOND};
    }

    if (config_.getValueView("request_timeout").hasValue()) {
        auto const requestTimeoutSecond = config_.getValue<uint32_t>("request_timeout");
        settings.requestTimeout = std::chrono::milliseconds{requestTimeoutSecond * util::MILLISECONDS_PER_SECOND};
    }

    settings.certificate = parseOptionalCertificate();
    settings.username = config_.maybeValue<std::string>("username");
    settings.password = config_.maybeValue<std::string>("password");

    return settings;
}

}  // namespace data::cassandra
