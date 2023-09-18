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

#include <data/cassandra/SettingsProvider.h>
#include <data/cassandra/impl/Cluster.h>
#include <data/cassandra/impl/Statement.h>
#include <util/config/Config.h>

#include <boost/json.hpp>

#include <fstream>
#include <string>
#include <thread>

namespace data::cassandra {

namespace detail {
inline Settings::ContactPoints
tag_invoke(boost::json::value_to_tag<Settings::ContactPoints>, boost::json::value const& value)
{
    if (not value.is_object())
        throw std::runtime_error(
            "Feed entire Cassandra section to parse "
            "Settings::ContactPoints instead");

    util::Config obj{value};
    Settings::ContactPoints out;

    out.contactPoints = obj.valueOrThrow<std::string>("contact_points", "`contact_points` must be a string");
    out.port = obj.maybeValue<uint16_t>("port");

    return out;
}

inline Settings::SecureConnectionBundle
tag_invoke(boost::json::value_to_tag<Settings::SecureConnectionBundle>, boost::json::value const& value)
{
    if (not value.is_string())
        throw std::runtime_error("`secure_connect_bundle` must be a string");
    return Settings::SecureConnectionBundle{value.as_string().data()};
}
}  // namespace detail

SettingsProvider::SettingsProvider(util::Config const& cfg, uint16_t ttl)
    : config_{cfg}
    , keyspace_{cfg.valueOr<std::string>("keyspace", "clio")}
    , tablePrefix_{cfg.maybeValue<std::string>("table_prefix")}
    , replicationFactor_{cfg.valueOr<uint16_t>("replication_factor", 3)}
    , ttl_{ttl}
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
    if (auto const certPath = config_.maybeValue<std::string>("certfile"); certPath)
    {
        auto const path = std::filesystem::path(*certPath);
        std::ifstream fileStream(path.string(), std::ios::in);
        if (!fileStream)
        {
            throw std::system_error(errno, std::generic_category(), "Opening certificate " + path.string());
        }

        std::string contents(std::istreambuf_iterator<char>{fileStream}, std::istreambuf_iterator<char>{});
        if (fileStream.bad())
        {
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
    if (auto const bundle = config_.maybeValue<Settings::SecureConnectionBundle>("secure_connect_bundle"); bundle)
    {
        settings.connectionInfo = *bundle;
    }
    else
    {
        settings.connectionInfo =
            config_.valueOrThrow<Settings::ContactPoints>("Missing contact_points in Cassandra config");
    }

    settings.threads = config_.valueOr<uint32_t>("threads", settings.threads);
    settings.maxWriteRequestsOutstanding =
        config_.valueOr<uint32_t>("max_write_requests_outstanding", settings.maxWriteRequestsOutstanding);
    settings.maxReadRequestsOutstanding =
        config_.valueOr<uint32_t>("max_read_requests_outstanding", settings.maxReadRequestsOutstanding);
    settings.coreConnectionsPerHost =
        config_.valueOr<uint32_t>("core_connections_per_host", settings.coreConnectionsPerHost);

    settings.queueSizeIO = config_.maybeValue<uint32_t>("queue_size_io");

    auto const connectTimeoutSecond = config_.maybeValue<uint32_t>("connect_timeout");
    if (connectTimeoutSecond)
        settings.connectionTimeout = std::chrono::milliseconds{*connectTimeoutSecond * 1000};

    auto const requestTimeoutSecond = config_.maybeValue<uint32_t>("request_timeout");
    if (requestTimeoutSecond)
        settings.requestTimeout = std::chrono::milliseconds{*requestTimeoutSecond * 1000};

    settings.certificate = parseOptionalCertificate();
    settings.username = config_.maybeValue<std::string>("username");
    settings.password = config_.maybeValue<std::string>("password");

    return settings;
}

}  // namespace data::cassandra
