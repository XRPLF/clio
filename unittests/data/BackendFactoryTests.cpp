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
#include "util/Fixtures.hpp"
#include "util/TestGlobals.hpp"
#include "util/config/Config.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <stdexcept>

namespace {
constexpr auto keyspace = "factory_test";
}  // namespace

class BackendCassandraFactoryTest : public SyncAsioContextTest {
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
    util::Config const cfg{boost::json::parse(
        R"({
            "database":
            {
                "type":"unknown"
            }
        })"
    )};
    EXPECT_THROW(make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTest, CreateCassandraBackendDBDisconnect)
{
    util::Config const cfg{boost::json::parse(fmt::format(
        R"({{
            "database":
            {{
                "type" : "cassandra",
                "cassandra" : {{
                    "contact_points": "{}",
                    "keyspace": "{}",
                    "replication_factor": 1,
                    "connect_timeout": 2
                }}
            }}
        }})",
        "127.0.0.2",
        keyspace
    ))};
    EXPECT_THROW(make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackend)
{
    util::Config const cfg{boost::json::parse(fmt::format(
        R"({{
            "database":
            {{
                "type": "cassandra",
                "cassandra": {{
                    "contact_points": "{}",
                    "keyspace": "{}",
                    "replication_factor": 1
                }}
            }}
        }})",
        TestGlobals::instance().backendHost,
        keyspace
    ))};

    {
        auto backend = make_Backend(cfg);
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
        auto backend = make_Backend(cfg);
        EXPECT_TRUE(backend);

        auto const range = backend->fetchLedgerRange();
        EXPECT_EQ(range->minSequence, 100);
        EXPECT_EQ(range->maxSequence, 500);
    }
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithEmptyDB)
{
    util::Config const cfg{boost::json::parse(fmt::format(
        R"({{
            "read_only": true,
            "database":
            {{
                "type" : "cassandra",
                "cassandra" : {{
                    "contact_points": "{}",
                    "keyspace": "{}",
                    "replication_factor": 1
                }}
            }}
        }})",
        TestGlobals::instance().backendHost,
        keyspace
    ))};
    EXPECT_THROW(make_Backend(cfg), std::runtime_error);
}

TEST_F(BackendCassandraFactoryTestWithDB, CreateCassandraBackendReadOnlyWithDBReady)
{
    util::Config const cfgReadOnly{boost::json::parse(fmt::format(
        R"({{
            "read_only": true,
            "database":
            {{
                "type" : "cassandra",
                "cassandra" : {{
                    "contact_points": "{}",
                    "keyspace": "{}",
                    "replication_factor": 1
                }}
            }}
        }})",
        TestGlobals::instance().backendHost,
        keyspace
    ))};

    util::Config const cfgWrite{boost::json::parse(fmt::format(
        R"({{
            "read_only": false,
            "database":
            {{
                "type" : "cassandra",
                "cassandra" : {{
                    "contact_points": "{}",
                    "keyspace": "{}",
                    "replication_factor": 1
                }}
            }}
        }})",
        TestGlobals::instance().backendHost,
        keyspace
    ))};

    EXPECT_TRUE(make_Backend(cfgWrite));
    EXPECT_TRUE(make_Backend(cfgReadOnly));
}
