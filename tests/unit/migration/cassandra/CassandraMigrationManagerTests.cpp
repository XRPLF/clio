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

#include "migration/MigrationManagerInterface.hpp"
#include "migration/MigratorsRegister.hpp"
#include "migration/TestMigrators.hpp"
#include "migration/cassandra/CassandraMigrationManager.hpp"
#include "util/MockMigrationBackend.hpp"
#include "util/MockMigrationBackendFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/Config.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>

using TestMigratorRegister =
    migration::MigratorsRegister<MockMigrationBackend, SimpleTestMigrator, RollbackableTestMigrator>;

using TestCassandraMigrationManager = migration::cassandra::CassandraMigrationManagerBase<TestMigratorRegister>;

struct CassandraMigrationManagerTest : public util::prometheus::WithMockPrometheus,
                                       public MockMigrationBackendTestStrict {
    util::Config cfg;

    std::shared_ptr<TestCassandraMigrationManager> migrationManager;

    CassandraMigrationManagerTest()
    {
        auto mockBackendPtr = backend.operator std::shared_ptr<MockMigrationBackend>();
        TestMigratorRegister migratorRegister(mockBackendPtr);
        migrationManager = std::make_shared<TestCassandraMigrationManager>(mockBackendPtr, cfg);
    }
};

TEST_F(CassandraMigrationManagerTest, AllStatus)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_))
        .WillOnce(testing::Return(std::unordered_set<std::string>{"SimpleTestMigrator"}));
    auto const status = migrationManager->allMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::Migrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::find(
            status.begin(),
            status.end(),
            std::make_tuple("RollbackableTestMigrator", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(CassandraMigrationManagerTest, AllNames)
{
    auto const names = migrationManager->allMigratorsNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "SimpleTestMigrator");
    EXPECT_EQ(names[1], "RollbackableTestMigrator");
}

TEST_F(CassandraMigrationManagerTest, RunMigration)
{
    EXPECT_CALL(*backend, writeMigratedMigrator("SimpleTestMigrator")).Times(1);
    migrationManager->runMigration("SimpleTestMigrator");
}

TEST_F(CassandraMigrationManagerTest, runRollback)
{
    EXPECT_CALL(*backend, removeMigratedMigrator("SimpleTestMigrator")).Times(1);
    migrationManager->runRollback("SimpleTestMigrator");
}

TEST_F(CassandraMigrationManagerTest, getMigratorStatusByName)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(std::unordered_set<std::string>{"SimpleTestMigrator"}));
    EXPECT_EQ(migrationManager->getMigratorStatusByName("SimpleTestMigrator"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(
        migrationManager->getMigratorStatusByName("RollbackableTestMigrator"), migration::MigratorStatus::NotMigrated
    );
}
