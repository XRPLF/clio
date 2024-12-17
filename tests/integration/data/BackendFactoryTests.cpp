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
#include "data/cassandra/Handle.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <TestGlobals.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace util::config;

struct BackendCassandraFactoryTest : SyncAsioContextTest, util::prometheus::WithPrometheus {
    constexpr static auto keyspace = "factory_test";

    ClioConfigDefinition cfg_{
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.port", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
        {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
        {"database.cassandra.threads",
         ConfigValue{ConfigType::Integer}.defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))},
        {"database.cassandra.core_connections_per_host", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
        {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.defaultValue(1).optional()},
        {"database.cassandra.request_timeout", ConfigValue{ConfigType::Integer}.defaultValue(1).optional()},
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
            std::ranges::for_each(*parseErrors, [](auto const& error) { std::cout << error.error << std::endl; });
            FAIL() << "Failed to parse config";
        }
    }
};

class BackendCassandraFactoryTestWithDB : public BackendCassandraFactoryTest {
protected:
    void
    TearDown() override
    {
        // drop the keyspace for next test
        data::cassandra::Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute("DROP KEYSPACE " + std::string{keyspace});
    }
};

TEST_F(BackendCassandraFactoryTest, NoSuchBackend)
{
    useConfig(R"json( {"database": {"type": "unknown"}} )json");
    EXPECT_THROW(data::make_Backend(cfg_), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTest, CreateCassandraBackendDBDisconnect)
{
    useConfig(R"json(
        {"database": {
            "type": "cassandra",
            "cassandra": {
                "contact_points": "127.0.0.2"
            }
        }}
    )json");

    EXPECT_THROW(data::make_Backend(cfg_), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackend)
{
    {
        auto backend = data::make_Backend(cfg_);
        EXPECT_TRUE(backend);

        // empty db does not have ledger range
        EXPECT_FALSE(backend->fetchLedgerRange());

        // insert range table
        data::cassandra::Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute(fmt::format("INSERT INTO {}.ledger_range (is_latest, sequence) VALUES (False, 100)", keyspace));
        handle.execute(fmt::format("INSERT INTO {}.ledger_range (is_latest, sequence) VALUES (True, 500)", keyspace));
    }

    {
        auto backend = data::make_Backend(cfg_);
        EXPECT_TRUE(backend);

        auto const range = backend->fetchLedgerRange();
        EXPECT_EQ(range->minSequence, 100);
        EXPECT_EQ(range->maxSequence, 500);
    }
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithEmptyDB)
{
    useConfig(R"json( {"read_only": true} )json");
    EXPECT_THROW(data::make_Backend(cfg_), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithDBReady)
{
    auto cfgReadOnly = cfg_;
    ASSERT_FALSE(cfgReadOnly.parse(ConfigFileJson{boost::json::parse(R"json( {"read_only": true} )json").as_object()}));

    EXPECT_TRUE(data::make_Backend(cfg_));
    EXPECT_TRUE(data::make_Backend(cfgReadOnly));
}
