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

#include "data/cassandra/Types.h"
#include <chrono>
#include <optional>
#include <util/Fixtures.h>
#include <util/TmpFile.h>

#include <data/cassandra/SettingsProvider.h>
#include <util/config/Config.h>

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <thread>
#include <variant>

using namespace util;
using namespace std;
namespace json = boost::json;

using namespace data::cassandra;

class SettingsProviderTest : public NoLoggerFixture {};

TEST_F(SettingsProviderTest, Defaults)
{
    Config const cfg{json::parse(R"({"contact_points": "127.0.0.1"})")};
    SettingsProvider const provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.threads, std::thread::hardware_concurrency());

    EXPECT_EQ(settings.enableLog, false);
    EXPECT_EQ(settings.connectionTimeout, std::chrono::milliseconds{10000});
    EXPECT_EQ(settings.requestTimeout, std::chrono::milliseconds{0});
    EXPECT_EQ(settings.maxWriteRequestsOutstanding, 10'000);
    EXPECT_EQ(settings.maxReadRequestsOutstanding, 100'000);
    EXPECT_EQ(settings.coreConnectionsPerHost, 1);
    EXPECT_EQ(settings.certificate, std::nullopt);
    EXPECT_EQ(settings.username, std::nullopt);
    EXPECT_EQ(settings.password, std::nullopt);
    EXPECT_EQ(settings.queueSizeIO, std::nullopt);

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
    Config const cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "port": 1234,
        "keyspace": "test",
        "replication_factor": 42,
        "table_prefix": "prefix",
        "threads": 24
    })")};
    SettingsProvider const provider{cfg};

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

TEST_F(SettingsProviderTest, DriverOptionalOptionsSpecified)
{
    Config const cfg{json::parse(R"({
        "contact_points": "123.123.123.123",
        "queue_size_io": 2
    })")};
    SettingsProvider const provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.queueSizeIO, 2);
}

TEST_F(SettingsProviderTest, SecureBundleConfig)
{
    Config const cfg{json::parse(R"({"secure_connect_bundle": "bundleData"})")};
    SettingsProvider const provider{cfg};

    auto const settings = provider.getSettings();
    auto const* sb = std::get_if<Settings::SecureConnectionBundle>(&settings.connectionInfo);
    ASSERT_TRUE(sb != nullptr);
    EXPECT_EQ(sb->bundle, "bundleData");
}

TEST_F(SettingsProviderTest, CertificateConfig)
{
    TmpFile const file{"certificateData"};
    Config const cfg{json::parse(fmt::format(
        R"({{
            "contact_points": "127.0.0.1",
            "certfile": "{}"
        }})",
        file.path
    ))};
    SettingsProvider const provider{cfg};

    auto const settings = provider.getSettings();
    EXPECT_EQ(settings.certificate, "certificateData");
}
