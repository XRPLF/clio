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

#include <backend/cassandra/Handle.h>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <semaphore>

using namespace clio;
using namespace std;

using namespace Backend::Cassandra;

namespace json = boost::json;

class BackendCassandraBaseTest : public NoLoggerFixture
{
protected:
    Handle
    createHandle(std::string_view contactPoints, std::string_view keyspace)
    {
        Handle handle{contactPoints};
        EXPECT_TRUE(handle.connect());
        auto const query = fmt::format(
            R"(
                CREATE KEYSPACE IF NOT EXISTS {} 
                  WITH replication = {{'class': 'SimpleStrategy', 'replication_factor': '1'}} 
                   AND durable_writes = true
            )",
            keyspace);
        EXPECT_TRUE(handle.execute(query));
        EXPECT_TRUE(handle.reconnect(keyspace));
        return handle;
    }

    void
    dropKeyspace(Handle const& handle, std::string_view keyspace)
    {
        std::string query = "DROP KEYSPACE " + std::string{keyspace};
        ASSERT_TRUE(handle.execute(query));
    }

    void
    prepStringsTable(Handle const& handle)
    {
        auto const entries = std::vector<std::string>{
            "first",
            "second",
            "third",
            "fourth",
            "fifth",
        };

        auto const q1 = fmt::format(
            R"(
                CREATE TABLE IF NOT EXISTS strings (hash blob PRIMARY KEY, sequence bigint)
                  WITH default_time_to_live = {}
            )",
            to_string(5000));

        auto const f1 = handle.asyncExecute(q1);
        auto const rc = f1.await();
        ASSERT_TRUE(rc) << rc.error();

        std::string q2 = "INSERT INTO strings (hash, sequence) VALUES (?, ?)";
        auto const insert = handle.prepare(q2);

        std::vector<Statement> statements;
        int64_t idx = 1000;

        for (auto const& entry : entries)
            statements.push_back(insert.bind(entry, static_cast<int64_t>(idx++)));

        EXPECT_EQ(statements.size(), entries.size());
        EXPECT_TRUE(handle.execute(statements));
    }
};

TEST_F(BackendCassandraBaseTest, ConnectionSuccess)
{
    Handle handle{"127.0.0.1"};
    auto const f = handle.asyncConnect();
    auto const res = f.await();

    ASSERT_TRUE(res);
}

TEST_F(BackendCassandraBaseTest, ConnectionFailFormat)
{
    Handle handle{"127.0.0."};
    auto const f = handle.asyncConnect();
    auto const res = f.await();

    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), "No hosts available: Unable to connect to any contact points");
    EXPECT_EQ(res.error().code(), CASS_ERROR_LIB_NO_HOSTS_AVAILABLE);
}

TEST_F(BackendCassandraBaseTest, ConnectionFailTimeout)
{
    Settings settings;
    settings.connectionTimeout = std::chrono::milliseconds{30};
    settings.connectionInfo = Settings::ContactPoints{"127.0.0.2"};

    Handle handle{settings};
    auto const f = handle.asyncConnect();
    auto const res = f.await();

    ASSERT_FALSE(res);

    // scylla and cassandra produce different text
    EXPECT_TRUE(res.error().message().starts_with("No hosts available: Underlying connection error:"));
    EXPECT_EQ(res.error().code(), CASS_ERROR_LIB_NO_HOSTS_AVAILABLE);
}

TEST_F(BackendCassandraBaseTest, FutureCallback)
{
    Handle handle{"127.0.0.1"};
    ASSERT_TRUE(handle.connect());

    auto const statement = handle.prepare("SELECT keyspace_name FROM system_schema.keyspaces").bind();

    bool complete = false;
    auto const f = handle.asyncExecute(statement, [&complete](auto const res) {
        complete = true;
        EXPECT_TRUE(res.value().hasRows());

        for (auto [ks] : extract<std::string>(res.value()))
            EXPECT_TRUE(not ks.empty());  // keyspace got some name
    });

    auto const res = f.await();  // callback should still be called
    ASSERT_TRUE(res);
    ASSERT_TRUE(complete);
}

TEST_F(BackendCassandraBaseTest, FutureCallbackSurviveMove)
{
    Handle handle{"127.0.0.1"};
    ASSERT_TRUE(handle.connect());

    auto const statement = handle.prepare("SELECT keyspace_name FROM system_schema.keyspaces").bind();

    bool complete = false;
    std::vector<FutureWithCallback> futures;
    std::binary_semaphore sem{0};

    futures.push_back(handle.asyncExecute(statement, [&complete, &sem](auto const res) {
        complete = true;
        EXPECT_TRUE(res.value().hasRows());

        for (auto [ks] : extract<std::string>(res.value()))
            EXPECT_TRUE(not ks.empty());  // keyspace got some name

        sem.release();
    }));

    sem.acquire();
    for (auto const& f : futures)
        ASSERT_TRUE(f.await());
    ASSERT_TRUE(complete);
}

TEST_F(BackendCassandraBaseTest, KeyspaceManipulation)
{
    Handle handle{"127.0.0.1"};
    std::string keyspace = "test_keyspace_manipulation";

    {
        auto const f = handle.asyncConnect(keyspace);
        auto const rc = f.await();
        ASSERT_FALSE(rc);  // initially expecting the keyspace does not exist
    }
    {
        auto const f = handle.asyncConnect();
        auto const rc = f.await();
        ASSERT_TRUE(rc);  // expect that we can still connect without keyspace
    }
    {
        const auto query = fmt::format(
            R"(
                CREATE KEYSPACE {} 
                  WITH replication = {{'class': 'SimpleStrategy', 'replication_factor': '1'}} 
                   AND durable_writes = true
            )",
            keyspace);
        auto const f = handle.asyncExecute(query);
        auto const rc = f.await();
        ASSERT_TRUE(rc);  // keyspace created
    }
    {
        auto const rc = handle.reconnect(keyspace);
        ASSERT_TRUE(rc);  // connect to the keyspace we created earlier
    }
    {
        auto const f = handle.asyncExecute("DROP KEYSPACE " + keyspace);
        auto const rc = f.await();
        ASSERT_TRUE(rc);  // dropped the keyspace
    }
    {
        auto const f = handle.asyncExecute("DROP KEYSPACE " + keyspace);
        auto const rc = f.await();
        ASSERT_FALSE(rc);  // keyspace already does not exist
    }
}

TEST_F(BackendCassandraBaseTest, CreateTableWithStrings)
{
    auto const entries = std::vector<std::string>{
        "first",
        "second",
        "third",
        "fourth",
        "fifth",
    };

    auto handle = createHandle("127.0.0.1", "test");
    auto q1 = fmt::format(
        R"(
            CREATE TABLE IF NOT EXISTS strings (hash blob PRIMARY KEY, sequence bigint) 
            WITH default_time_to_live = {}
        )",
        5000);

    auto const f1 = handle.asyncExecute(q1);
    auto const rc = f1.await();
    ASSERT_TRUE(rc) << rc.error();

    std::string q2 = "INSERT INTO strings (hash, sequence) VALUES (?, ?)";
    auto insert = handle.prepare(q2);

    // write data
    {
        std::vector<Future> futures;
        int64_t idx = 1000;

        for (auto const& entry : entries)
            futures.push_back(handle.asyncExecute(insert, entry, static_cast<int64_t>(idx++)));

        ASSERT_EQ(futures.size(), entries.size());
        for (auto const& f : futures)
        {
            auto const rc = f.await();
            ASSERT_TRUE(rc) << rc.error();
        }
    }

    // read data back
    {
        auto const res = handle.execute("SELECT hash, sequence FROM strings");
        ASSERT_TRUE(res) << res.error();

        auto const& results = res.value();
        auto const totalRows = results.numRows();
        EXPECT_EQ(totalRows, entries.size());

        for (auto [hash, seq] : extract<std::string, int64_t>(results))
            EXPECT_TRUE(std::find(std::begin(entries), std::end(entries), hash) != std::end(entries));
    }

    // delete everything
    {
        auto const res = handle.execute("DROP TABLE strings");
        ASSERT_TRUE(res) << res.error();
        dropKeyspace(handle, "test");
    }
}

TEST_F(BackendCassandraBaseTest, BatchInsert)
{
    auto const entries = std::vector<std::string>{
        "first",
        "second",
        "third",
        "fourth",
        "fifth",
    };

    auto handle = createHandle("127.0.0.1", "test");
    auto const q1 = fmt::format(
        R"(
            CREATE TABLE IF NOT EXISTS strings (hash blob PRIMARY KEY, sequence bigint) 
              WITH default_time_to_live = {}
        )",
        5000);
    auto const f1 = handle.asyncExecute(q1);
    auto const rc = f1.await();
    ASSERT_TRUE(rc) << rc.error();

    std::string q2 = "INSERT INTO strings (hash, sequence) VALUES (?, ?)";
    auto const insert = handle.prepare(q2);

    // write data in bulk
    {
        std::vector<Statement> statements;
        int64_t idx = 1000;

        for (auto const& entry : entries)
            statements.push_back(insert.bind(entry, static_cast<int64_t>(idx++)));

        ASSERT_EQ(statements.size(), entries.size());

        auto const rc = handle.execute(statements);
        ASSERT_TRUE(rc) << rc.error();
    }

    // read data back
    {
        auto const res = handle.execute("SELECT hash, sequence FROM strings");
        ASSERT_TRUE(res) << res.error();

        auto const& results = res.value();
        auto const totalRows = results.numRows();
        EXPECT_EQ(totalRows, entries.size());

        for (auto [hash, seq] : extract<std::string, int64_t>(results))
            EXPECT_TRUE(std::find(std::begin(entries), std::end(entries), hash) != std::end(entries));
    }

    dropKeyspace(handle, "test");
}

TEST_F(BackendCassandraBaseTest, BatchInsertAsync)
{
    using std::to_string;
    auto const entries = std::vector<std::string>{
        "first",
        "second",
        "third",
        "fourth",
        "fifth",
    };

    auto handle = createHandle("127.0.0.1", "test");
    auto const q1 = fmt::format(
        R"(
            CREATE TABLE IF NOT EXISTS strings (hash blob PRIMARY KEY, sequence bigint) 
              WITH default_time_to_live = {}
        )",
        5000);
    auto const f1 = handle.asyncExecute(q1);
    auto const rc = f1.await();
    ASSERT_TRUE(rc) << rc.error();

    std::string q2 = "INSERT INTO strings (hash, sequence) VALUES (?, ?)";
    auto const insert = handle.prepare(q2);

    // write data in bulk
    {
        bool complete = false;
        std::optional<Backend::Cassandra::FutureWithCallback> fut;

        {
            std::vector<Statement> statements;
            int64_t idx = 1000;

            for (auto const& entry : entries)
                statements.push_back(insert.bind(entry, static_cast<int64_t>(idx++)));

            ASSERT_EQ(statements.size(), entries.size());
            fut.emplace(handle.asyncExecute(statements, [&](auto const res) {
                complete = true;
                EXPECT_TRUE(res);
            }));
            // statements are destructed here, async execute needs to survive
        }

        auto const res = fut.value().await();  // future should still signal it finished
        EXPECT_TRUE(res);
        ASSERT_TRUE(complete);
    }

    dropKeyspace(handle, "test");
}

TEST_F(BackendCassandraBaseTest, AlterTableAddColumn)
{
    auto handle = createHandle("127.0.0.1", "test");
    auto const q1 = fmt::format(
        R"(
            CREATE TABLE IF NOT EXISTS strings (hash blob PRIMARY KEY, sequence bigint) 
              WITH default_time_to_live = {}
        )",
        5000);
    ASSERT_TRUE(handle.execute(q1));

    std::string update = "ALTER TABLE strings ADD tmp blob";
    ASSERT_TRUE(handle.execute(update));

    dropKeyspace(handle, "test");
}

TEST_F(BackendCassandraBaseTest, AlterTableMoveToNewTable)
{
    auto handle = createHandle("127.0.0.1", "test");
    prepStringsTable(handle);

    auto const newTable = fmt::format(
        R"(
            CREATE TABLE IF NOT EXISTS strings_v2 (hash blob PRIMARY KEY, sequence bigint, tmp bigint) 
              WITH default_time_to_live = {}
        )",
        5000);
    ASSERT_TRUE(handle.execute(newTable));

    // now migrate data; tmp column will just get the sequence number + 1 stored
    std::vector<Statement> migrationStatements;
    auto const migrationInsert = handle.prepare("INSERT INTO strings_v2 (hash, sequence, tmp) VALUES (?, ?, ?)");

    auto const res = handle.execute("SELECT hash, sequence FROM strings");
    ASSERT_TRUE(res);

    auto const& results = res.value();
    for (auto [hash, seq] : extract<std::string, int64_t>(results))
    {
        static_assert(std::is_same_v<decltype(hash), std::string>);
        static_assert(std::is_same_v<decltype(seq), int64_t>);
        migrationStatements.push_back(
            migrationInsert.bind(hash, static_cast<int64_t>(seq), static_cast<int64_t>(seq + 1u)));
    }

    EXPECT_TRUE(handle.execute(migrationStatements));

    // now let's read back the v2 table and compare
    auto const resV2 = handle.execute("SELECT sequence, tmp FROM strings_v2");
    EXPECT_TRUE(resV2);
    auto const& resultsV2 = resV2.value();

    EXPECT_EQ(results.numRows(), resultsV2.numRows());
    for (auto [seq, tmp] : extract<int64_t, int64_t>(resultsV2))
    {
        static_assert(std::is_same_v<decltype(seq), int64_t>);
        static_assert(std::is_same_v<decltype(tmp), int64_t>);
        EXPECT_EQ(seq + 1, tmp);
    }

    dropKeyspace(handle, "test");
}
