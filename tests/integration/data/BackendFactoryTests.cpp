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

#include "data/BackendFactory.hpp"
#include "data/LedgerCache.hpp"
#include "data/cassandra/Handle.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <TestGlobals.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace util::config;

struct BackendCassandraFactoryTest : SyncAsioContextTest, util::prometheus::WithPrometheus {
    static constexpr auto kKEYSPACE = "factory_test";
    static constexpr auto kPROVIDER = "cassandra";

protected:
    ClioConfigDefinition cfg_{
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.port", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(kKEYSPACE)},
        {"database.cassandra.provider", ConfigValue{ConfigType::String}.defaultValue(kPROVIDER)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.max_write_requests_outstanding",
         ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
        {"database.cassandra.max_read_requests_outstanding",
         ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
        {"database.cassandra.threads",
         ConfigValue{ConfigType::Integer}.defaultValue(
             static_cast<uint32_t>(std::thread::hardware_concurrency())
         )},
        {"database.cassandra.core_connections_per_host",
         ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
        {"database.cassandra.connect_timeout",
         ConfigValue{ConfigType::Integer}.defaultValue(1).optional()},
        {"database.cassandra.request_timeout", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},

        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)}
    };

    void
    useConfig(std::string config)
    {
        auto jsonConfig = boost::json::parse(config).as_object();
        auto const parseErrors = cfg_.parse(ConfigFileJson{jsonConfig});
        if (parseErrors) {
            std::ranges::for_each(*parseErrors, [](auto const& error) {
                std::cout << error.error << std::endl;
            });
            FAIL() << "Failed to parse config";
        }
    }
};

class BackendCassandraFactoryTestWithDB : public BackendCassandraFactoryTest {
public:
    ~BackendCassandraFactoryTestWithDB() override
    {
        // drop the keyspace for next test
        data::cassandra::Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute("DROP KEYSPACE " + std::string{kKEYSPACE});
    }
};

TEST_F(BackendCassandraFactoryTest, NoSuchBackend)
{
    useConfig(R"JSON( {"database": {"type": "unknown"}} )JSON");
    auto cache = data::LedgerCache{};
    EXPECT_THROW(data::makeBackend(cfg_, cache), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTest, CreateCassandraBackendDBDisconnect)
{
    useConfig(R"JSON(
        {"database": {
            "type": "cassandra",
            "cassandra": {
                "contact_points": "127.0.0.2"
            }
        }}
    )JSON");

    auto cache = data::LedgerCache{};
    EXPECT_THROW(data::makeBackend(cfg_, cache), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackend)
{
    {
        auto cache = data::LedgerCache{};
        auto backend = data::makeBackend(cfg_, cache);
        EXPECT_TRUE(backend);

        // empty db does not have ledger range
        EXPECT_FALSE(backend->fetchLedgerRange());

        // insert range table
        data::cassandra::Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute(
            fmt::format(
                "INSERT INTO {}.ledger_range (is_latest, sequence) VALUES (False, 100)", kKEYSPACE
            )
        );
        handle.execute(
            fmt::format(
                "INSERT INTO {}.ledger_range (is_latest, sequence) VALUES (True, 500)", kKEYSPACE
            )
        );
    }

    {
        auto cache = data::LedgerCache{};
        auto backend = data::makeBackend(cfg_, cache);
        EXPECT_TRUE(backend);

        auto const range = backend->fetchLedgerRange();
        EXPECT_EQ(range->minSequence, 100);
        EXPECT_EQ(range->maxSequence, 500);
    }
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithEmptyDB)
{
    useConfig(R"JSON( {"read_only": true} )JSON");
    auto cache = data::LedgerCache{};
    EXPECT_THROW(data::makeBackend(cfg_, cache), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithDBReady)
{
    auto cfgReadOnly = cfg_;
    ASSERT_FALSE(cfgReadOnly.parse(
        ConfigFileJson{boost::json::parse(R"JSON( {"read_only": true} )JSON").as_object()}
    ));

    auto cache = data::LedgerCache{};
    EXPECT_TRUE(data::makeBackend(cfg_, cache));
    EXPECT_TRUE(data::makeBackend(cfgReadOnly, cache));
}
