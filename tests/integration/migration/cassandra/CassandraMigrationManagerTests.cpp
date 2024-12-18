//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "migration/cassandra/DBRawData.hpp"
#include "migration/cassandra/ExampleDropTableMigrator.hpp"
#include "migration/cassandra/ExampleLedgerMigrator.hpp"
#include "migration/cassandra/ExampleObjectsMigrator.hpp"
#include "migration/cassandra/ExampleTransactionsMigrator.hpp"
#include "migration/impl/MigrationManagerBase.hpp"
#include "migration/impl/MigratorsRegister.hpp"
#include "util/CassandraDBHelper.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <TestGlobals.hpp>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

using namespace util;
using namespace std;
using namespace prometheus;
using namespace data::cassandra;
using namespace migration;
using namespace util::config;

// Register the migrators
using CassandraSupportedTestMigrators = migration::impl::MigratorsRegister<
    CassandraMigrationTestBackend,
    ExampleObjectsMigrator,
    ExampleTransactionsMigrator,
    ExampleLedgerMigrator,
    ExampleDropTableMigrator>;

// Define the test migration manager
using CassandraMigrationTestManager = migration::impl::MigrationManagerBase<CassandraSupportedTestMigrators>;

namespace {
std::pair<std::shared_ptr<migration::MigrationManagerInterface>, std::shared_ptr<CassandraMigrationTestBackend>>
make_MigrationTestManagerAndBackend(ClioConfigDefinition const& config)
{
    auto const cfg = config.getObject("database.cassandra");

    auto const backendPtr = std::make_shared<CassandraMigrationTestBackend>(data::cassandra::SettingsProvider{cfg});

    return std::make_pair(
        std::make_shared<CassandraMigrationTestManager>(backendPtr, config.getObject("migration")), backendPtr
    );
}
}  // namespace

class MigrationCassandraSimpleTest : public WithPrometheus, public NoLoggerFixture {
    // This function is used to prepare the database before running the tests
    // It is called in the SetUp function. Different tests can override this function to prepare the database
    // differently
    virtual void
    setupDatabase()
    {
    }

protected:
    ClioConfigDefinition cfg{

        {{"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
         {"database.cassandra.contact_points",
          ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
         {"database.cassandra.keyspace",
          ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendKeyspace)},
         {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
         {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
         {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.port", ConfigValue{ConfigType::Integer}.withConstraint(validatePort).optional()},
         {"database.cassandra.replication_factor",
          ConfigValue{ConfigType::Integer}.defaultValue(3u).withConstraint(validateUint16)},
         {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.max_write_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(10'000).withConstraint(validateUint32)},
         {"database.cassandra.max_read_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(100'000).withConstraint(validateUint32)},
         {"database.cassandra.threads",
          ConfigValue{ConfigType::Integer}
              .defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))
              .withConstraint(validateUint32)},
         {"database.cassandra.core_connections_per_host",
          ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(validateUint16)},
         {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint16)
         },
         {"database.cassandra.write_batch_size",
          ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(validateUint16)},
         {"database.cassandra.connect_timeout",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
         {"database.cassandra.request_timeout",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
         {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},
         {"migration.full_scan_threads", ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(validateUint32)
         },
         {"migration.full_scan_jobs", ConfigValue{ConfigType::Integer}.defaultValue(4).withConstraint(validateUint32)},
         {"migration.cursors_per_job", ConfigValue{ConfigType::Integer}.defaultValue(100).withConstraint(validateUint32)
         }}
    };

    std::shared_ptr<migration::MigrationManagerInterface> testMigrationManager;
    std::shared_ptr<CassandraMigrationTestBackend> testMigrationBackend;

    MigrationCassandraSimpleTest()
    {
        auto const testBundle = make_MigrationTestManagerAndBackend(cfg);
        testMigrationManager = testBundle.first;
        testMigrationBackend = testBundle.second;
    }

    void
    SetUp() override
    {
        setupDatabase();
    }

    void
    TearDown() override
    {
        // drop the keyspace
        Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        handle.execute("DROP KEYSPACE " + TestGlobals::instance().backendKeyspace);
    }
};

// The test suite for testing the migration manager without any data in the database
struct MigrationCassandraManagerCleanDBTest : public MigrationCassandraSimpleTest {};

TEST_F(MigrationCassandraManagerCleanDBTest, GetAllMigratorNames)
{
    auto const names = testMigrationManager->allMigratorsNames();
    EXPECT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "ExampleObjectsMigrator");
    EXPECT_EQ(names[1], "ExampleTransactionsMigrator");
    EXPECT_EQ(names[2], "ExampleLedgerMigrator");
    EXPECT_EQ(names[3], "ExampleDropTableMigrator");
}

TEST_F(MigrationCassandraManagerCleanDBTest, AllMigratorStatusBeforeAnyMigration)
{
    auto const status = testMigrationManager->allMigratorsStatusPairs();
    EXPECT_EQ(status.size(), 4);
    EXPECT_EQ(std::get<1>(status[0]), MigratorStatus::NotMigrated);
    EXPECT_EQ(std::get<1>(status[1]), MigratorStatus::NotMigrated);
    EXPECT_EQ(std::get<1>(status[2]), MigratorStatus::NotMigrated);
    EXPECT_EQ(std::get<1>(status[3]), MigratorStatus::NotMigrated);
}

TEST_F(MigrationCassandraManagerCleanDBTest, MigratorStatus)
{
    auto status = testMigrationManager->getMigratorStatusByName("ExampleObjectsMigrator");
    EXPECT_EQ(status, MigratorStatus::NotMigrated);

    status = testMigrationManager->getMigratorStatusByName("ExampleTransactionsMigrator");
    EXPECT_EQ(status, MigratorStatus::NotMigrated);

    status = testMigrationManager->getMigratorStatusByName("ExampleLedgerMigrator");
    EXPECT_EQ(status, MigratorStatus::NotMigrated);

    status = testMigrationManager->getMigratorStatusByName("ExampleDropTableMigrator");
    EXPECT_EQ(status, MigratorStatus::NotMigrated);

    status = testMigrationManager->getMigratorStatusByName("NonExistentMigrator");
    EXPECT_EQ(status, MigratorStatus::NotKnown);
}

// The test suite for testing migration process for ExampleTransactionsMigrator. In this test suite, the transactions
// are written to the database before running the migration
class MigrationCassandraManagerTxTableTest : public MigrationCassandraSimpleTest {
    void
    setupDatabase() override
    {
        Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());

        std::ranges::for_each(TransactionsRawData, [&](auto const& value) {
            writeTxFromCSVString(TestGlobals::instance().backendKeyspace, value, handle);
        });
    }
};

TEST_F(MigrationCassandraManagerTxTableTest, MigrateExampleTransactionsMigrator)
{
    auto constexpr TransactionsMigratorName = "ExampleTransactionsMigrator";
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(TransactionsMigratorName), MigratorStatus::NotMigrated);

    ExampleTransactionsMigrator::count = 0;
    testMigrationManager->runMigration(TransactionsMigratorName);
    EXPECT_EQ(ExampleTransactionsMigrator::count, TransactionsRawData.size());

    auto const newTableSize =
        data::synchronous([&](auto ctx) { return testMigrationBackend->fetchTxIndexTableSize(ctx); });

    EXPECT_TRUE(newTableSize.has_value());
    EXPECT_EQ(newTableSize, TransactionsRawData.size());

    // check a few tx types
    auto const getTxType = [&](ripple::uint256 const& txHash) -> std::optional<std::string> {
        return data::synchronous([&](auto ctx) {
            return testMigrationBackend->fetchTxTypeViaID(uint256ToString(txHash), ctx);
        });
    };

    auto txType = getTxType(ripple::uint256("CEECF7E516F8A53C5D32A357B737ED54D3186FDD510B1973D908AD8D93AD8E00"));
    EXPECT_TRUE(txType.has_value());
    EXPECT_EQ(txType.value(), "OracleSet");

    txType = getTxType(ripple::uint256("35DBFB1A88DE17EBD2BCE37F6E1FD6D3B9887C92B7933ED2FCF2A84E9138B7CA"));
    EXPECT_TRUE(txType.has_value());
    EXPECT_EQ(txType.value(), "Payment");

    txType = getTxType(ripple::uint256("FCACE9D00625FA3BCC5316078324EA153EC8551243AC1701D496CC1CA2B8A474"));
    EXPECT_TRUE(txType.has_value());
    EXPECT_EQ(txType.value(), "AMMCreate");

    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(TransactionsMigratorName), MigratorStatus::Migrated);
}

// The test suite for testing migration process for ExampleObjectsMigrator. In this test suite, the objects are written
// to the database before running the migration
class MigrationCassandraManagerObjectsTableTest : public MigrationCassandraSimpleTest {
    void
    setupDatabase() override
    {
        Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        for (auto const& value : ObjectsRawData) {
            writeObjectFromCSVString(TestGlobals::instance().backendKeyspace, value, handle);
        }
    }
};

TEST_F(MigrationCassandraManagerObjectsTableTest, MigrateExampleObjectsMigrator)
{
    auto constexpr ObjectsMigratorName = "ExampleObjectsMigrator";
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(ObjectsMigratorName), MigratorStatus::NotMigrated);

    testMigrationManager->runMigration(ObjectsMigratorName);

    EXPECT_EQ(ExampleObjectsMigrator::count, ObjectsRawData.size());
    EXPECT_EQ(ExampleObjectsMigrator::accountCount, 37);

    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(ObjectsMigratorName), MigratorStatus::Migrated);
}

// The test suite for testing migration process for ExampleLedgerMigrator. In this test suite, the ledger headers are
// written to ledgers table before running the migration
class MigrationCassandraManagerLedgerTableTest : public MigrationCassandraSimpleTest {
    void
    setupDatabase() override
    {
        Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        for (auto const& value : LedgerHeaderRawData) {
            writeLedgerFromCSVString(TestGlobals::instance().backendKeyspace, value, handle);
        }
        writeLedgerRange(TestGlobals::instance().backendKeyspace, 5619393, 5619442, handle);
    }
};

TEST_F(MigrationCassandraManagerLedgerTableTest, MigrateExampleLedgerMigrator)
{
    auto constexpr HeaderMigratorName = "ExampleLedgerMigrator";
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(HeaderMigratorName), MigratorStatus::NotMigrated);

    testMigrationManager->runMigration(HeaderMigratorName);
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(HeaderMigratorName), MigratorStatus::Migrated);

    auto const newTableSize =
        data::synchronous([&](auto ctx) { return testMigrationBackend->fetchLedgerTableSize(ctx); });
    EXPECT_EQ(newTableSize, LedgerHeaderRawData.size());

    auto const getAccountHash = [this](std::uint32_t seq) {
        return data::synchronous([&](auto ctx) { return testMigrationBackend->fetchAccountHashViaSequence(seq, ctx); });
    };

    EXPECT_EQ(
        getAccountHash(5619393), ripple::uint256("D1DE0F83A6858DF52811E31FE97B8449A4DD55A7D9E0023FE5DC2B335E4C49E8")
    );
    EXPECT_EQ(
        getAccountHash(5619394), ripple::uint256("3FEF485204772F03842AA8757B4631E8F146E17AD9762E0552540A517DD38A24")
    );
    EXPECT_EQ(
        getAccountHash(5619395), ripple::uint256("D0A61C158AD8941868666AD51C4662EEAAA2A141BF0F4435BC22B9BC6783AF65")
    );
}

// The test suite for testing migration process for ExampleDropTableMigrator.
class MigrationCassandraManagerDropTableTest : public MigrationCassandraSimpleTest {};

TEST_F(MigrationCassandraManagerDropTableTest, MigrateDropTableMigrator)
{
    auto constexpr DropTableMigratorName = "ExampleDropTableMigrator";
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(DropTableMigratorName), MigratorStatus::NotMigrated);

    auto const beforeDropSize =
        data::synchronous([&](auto ctx) { return testMigrationBackend->fetchDiffTableSize(ctx); });
    EXPECT_EQ(beforeDropSize, 0);

    testMigrationManager->runMigration(DropTableMigratorName);
    EXPECT_EQ(testMigrationManager->getMigratorStatusByName(DropTableMigratorName), MigratorStatus::Migrated);

    auto const newTableSize =
        data::synchronous([&](auto ctx) { return testMigrationBackend->fetchDiffTableSize(ctx); });
    EXPECT_EQ(newTableSize, std::nullopt);
}
