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

#include <util/Fixtures.h>
#include <util/TmpFile.h>

#include <backend/cassandra/SettingsProvider.h>
#include <config/Config.h>

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <thread>
#include <variant>

using namespace clio;
using namespace std;
namespace json = boost::json;

using namespace Backend::Cassandra;

class SettingsProviderTest : public NoLoggerFixture
{
};

TEST_F(SettingsProviderTest, Defaults)
{
    Config cfg{json::parse(R"({"contact_points": "127.0.0.1"})")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.threads, std::thread::hardware_concurrency());

    EXPECT_EQ(settings.enableLog, false);
    EXPECT_EQ(settings.connectionTimeout, std::chrono::milliseconds{10000});
    EXPECT_EQ(settings.requestTimeout, std::chrono::milliseconds{0});
    EXPECT_EQ(settings.maxWriteRequestsOutstanding, 10'000);
    EXPECT_EQ(settings.maxReadRequestsOutstanding, 100'000);
    EXPECT_EQ(settings.maxConnectionsPerHost, 2);
    EXPECT_EQ(settings.coreConnectionsPerHost, 2);
    EXPECT_EQ(settings.maxConcurrentRequestsThreshold, (100'000 + 10'000) / 2);
    EXPECT_EQ(settings.certificate, std::nullopt);
    EXPECT_EQ(settings.username, std::nullopt);
    EXPECT_EQ(settings.password, std::nullopt);
    EXPECT_EQ(settings.queueSizeIO, std::nullopt);
    EXPECT_EQ(settings.queueSizeEvent, std::nullopt);
    EXPECT_EQ(settings.writeBytesHighWatermark, std::nullopt);
    EXPECT_EQ(settings.writeBytesLowWatermark, std::nullopt);
    EXPECT_EQ(settings.pendingRequestsHighWatermark, std::nullopt);
    EXPECT_EQ(settings.pendingRequestsLowWatermark, std::nullopt);
    EXPECT_EQ(settings.maxRequestsPerFlush, std::nullopt);
    EXPECT_EQ(settings.maxConcurrentCreation, std::nullopt);

    auto const* cp = std::get_if<Settings::ContactPoints>(&settings.connectionInfo);
    ASSERT_TRUE(cp != nullptr);
    EXPECT_EQ(cp->contactPoints, "127.0.0.1");
    EXPECT_FALSE(cp->port);

    EXPECT_EQ(provider.getKeyspace(), "clio");
    EXPECT_EQ(provider.getReplicationFactor(), 3);
    EXPECT_EQ(provider.getTablePrefix(), std::nullopt);
}

TEST_F(SettingsProviderTest, SimpleConfig)
{
    Config cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "port": 1234,
        "keyspace": "test",
        "replication_factor": 42,
        "table_prefix": "prefix",
        "threads": 24
    })")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.threads, 24);

    auto const* cp = std::get_if<Settings::ContactPoints>(&settings.connectionInfo);
    ASSERT_TRUE(cp != nullptr);
    EXPECT_EQ(cp->contactPoints, "123.123.123.123");
    EXPECT_EQ(cp->port, 1234);

    EXPECT_EQ(provider.getKeyspace(), "test");
    EXPECT_EQ(provider.getReplicationFactor(), 42);
    EXPECT_EQ(provider.getTablePrefix(), "prefix");
}

TEST_F(SettingsProviderTest, DriverOptionCalculation)
{
    Config cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "max_write_requests_outstanding": 100,
        "max_read_requests_outstanding": 200
    })")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.maxReadRequestsOutstanding, 200);
    EXPECT_EQ(settings.maxWriteRequestsOutstanding, 100);

    EXPECT_EQ(settings.maxConnectionsPerHost, 2);
    EXPECT_EQ(settings.coreConnectionsPerHost, 2);
    EXPECT_EQ(settings.maxConcurrentRequestsThreshold, 150);  // calculated from above
}

TEST_F(SettingsProviderTest, DriverOptionSecifiedMaxConcurrentRequestsThreshold)
{
    Config cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "max_write_requests_outstanding": 100,
        "max_read_requests_outstanding": 200,
        "max_connections_per_host": 5,
        "core_connections_per_host": 4,
        "max_concurrent_requests_threshold": 1234
    })")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.maxReadRequestsOutstanding, 200);
    EXPECT_EQ(settings.maxWriteRequestsOutstanding, 100);

    EXPECT_EQ(settings.maxConnectionsPerHost, 5);
    EXPECT_EQ(settings.coreConnectionsPerHost, 4);
    EXPECT_EQ(settings.maxConcurrentRequestsThreshold, 1234);
}

TEST_F(SettingsProviderTest, DriverOptionalOptionsSpecified)
{
    Config cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "queue_size_event": 1,
        "queue_size_io": 2,
        "write_bytes_high_water_mark": 3,
        "write_bytes_low_water_mark": 4,
        "pending_requests_high_water_mark": 5,
        "pending_requests_low_water_mark": 6,
        "max_requests_per_flush": 7,
        "max_concurrent_creation": 8
    })")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.queueSizeEvent, 1);
    EXPECT_EQ(settings.queueSizeIO, 2);
    EXPECT_EQ(settings.writeBytesHighWatermark, 3);
    EXPECT_EQ(settings.writeBytesLowWatermark, 4);
    EXPECT_EQ(settings.pendingRequestsHighWatermark, 5);
    EXPECT_EQ(settings.pendingRequestsLowWatermark, 6);
    EXPECT_EQ(settings.maxRequestsPerFlush, 7);
    EXPECT_EQ(settings.maxConcurrentCreation, 8);
}

TEST_F(SettingsProviderTest, SecureBundleConfig)
{
    Config cfg{json::parse(R"({"secure_connect_bundle": "bundleData"})")};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    auto const* sb = std::get_if<Settings::SecureConnectionBundle>(&settings.connectionInfo);
    ASSERT_TRUE(sb != nullptr);
    EXPECT_EQ(sb->bundle, "bundleData");
}

TEST_F(SettingsProviderTest, CertificateConfig)
{
    TmpFile file{"certificateData"};
    Config cfg{json::parse(fmt::format(
        R"({{
            "contact_points": "127.0.0.1",
            "certfile": "{}"
        }})",
        file.path))};
    SettingsProvider provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.certificate, "certificateData");
}
