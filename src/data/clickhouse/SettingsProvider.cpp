//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/clickhouse/SettingsProvider.hpp"
#include "util/log/Logger.hpp"

#include <chrono>

namespace data::clickhouse {

SettingsProvider::SettingsProvider(util::config::ObjectView const& cfg) : config_{cfg}
{
    database_ = config_.get<std::string>("database");
    if (auto const prefix = config_.maybeValue<std::string>("table_prefix")) {
        tablePrefix_ = *prefix;
    }
}

Settings
SettingsProvider::getSettings() const
{
    return parseSettings();
}

Settings
SettingsProvider::parseSettings() const
{
    Settings settings;

    // Parse connection settings
    if (auto const host = config_.maybeValue<std::string>("host")) {
        settings.connectionInfo.host = *host;
    }

    if (auto const port = config_.maybeValue<uint16_t>("port")) {
        settings.connectionInfo.port = *port;
    }

    // Use the database from constructor or default
    settings.connectionInfo.database = database_;

    // Parse timeout settings
    if (auto const connectTimeout = config_.maybeValue<uint32_t>("connect_timeout")) {
        settings.connectionTimeout = std::chrono::milliseconds{*connectTimeout};
    }

    if (auto const requestTimeout = config_.maybeValue<uint32_t>("request_timeout")) {
        settings.requestTimeout = std::chrono::milliseconds{*requestTimeout};
    }

    // Parse thread and connection settings
    if (auto const threads = config_.maybeValue<uint32_t>("threads")) {
        settings.threads = *threads;
    }

    if (auto const maxWriteRequests = config_.maybeValue<uint32_t>("max_write_requests_outstanding")) {
        settings.maxWriteRequestsOutstanding = *maxWriteRequests;
    }

    if (auto const maxReadRequests = config_.maybeValue<uint32_t>("max_read_requests_outstanding")) {
        settings.maxReadRequestsOutstanding = *maxReadRequests;
    }

    if (auto const coreConnections = config_.maybeValue<uint32_t>("core_connections_per_host")) {
        settings.coreConnectionsPerHost = *coreConnections;
    }

    if (auto const batchSize = config_.maybeValue<uint32_t>("write_batch_size")) {
        settings.writeBatchSize = *batchSize;
    }

    if (auto const queueSize = config_.maybeValue<uint32_t>("queue_size_io")) {
        settings.queueSizeIO = *queueSize;
    }

    // Parse authentication settings
    if (auto const username = config_.maybeValue<std::string>("username")) {
        settings.username = *username;
    }

    if (auto const password = config_.maybeValue<std::string>("password")) {
        settings.password = *password;
    }

    // Parse logging settings
    if (auto const enableLog = config_.maybeValue<bool>("enable_log")) {
        settings.enableLog = *enableLog;
    }

    return settings;
}

}  // namespace data::clickhouse
