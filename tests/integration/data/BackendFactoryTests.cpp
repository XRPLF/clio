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
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <TestGlobals.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace util::config;

namespace {
constexpr auto keyspace = "factory_test";
}  // namespace

class BackendCassandraFactoryTest : public SyncAsioContextTest, public util::prometheus::WithPrometheus {
protected:
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
    }

    void
    TearDown() override
    {
        SyncAsioContextTest::TearDown();
    }
};

class BackendCassandraFactoryTestWithDB : public BackendCassandraFactoryTest {
protected:
    void
    SetUp() override
    {
        BackendCassandraFactoryTest::SetUp();
    }

    void
    TearDown() override
    {
        BackendCassandraFactoryTest::TearDown();
        // drop the keyspace for next test
        data::cassandra::Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute("DROP KEYSPACE " + std::string{keyspace});
    }
};

TEST_F(BackendCassandraFactoryTest, NoSuchBackend)
{
    ClioConfigDefinition cfg{{"database.type", ConfigValue{ConfigType::String}.defaultValue("unknown")}};
    EXPECT_THROW(data::make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTest, CreateCassandraBackendDBDisconnect)
{
    ClioConfigDefinition cfg{
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("127.0.0.2")},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.defaultValue(2)}
    };
    EXPECT_THROW(data::make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackend)
{
    ClioConfigDefinition cfg{
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)}
    };

    {
        auto backend = data::make_Backend(cfg);
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
        auto backend = data::make_Backend(cfg);
        EXPECT_TRUE(backend);

        auto const range = backend->fetchLedgerRange();
        EXPECT_EQ(range->minSequence, 100);
        EXPECT_EQ(range->maxSequence, 500);
    }
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithEmptyDB)
{
    ClioConfigDefinition cfg{
        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)}
    };

    EXPECT_THROW(data::make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithDBReady)
{
    ClioConfigDefinition cfgReadOnly{
        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)}
    };

    ClioConfigDefinition cfgWrite{
        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
        {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
        {"database.cassandra.contact_points",
         ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue(keyspace)},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)}
    };

    EXPECT_TRUE(data::make_Backend(cfgWrite));
    EXPECT_TRUE(data::make_Backend(cfgReadOnly));
}
