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

#include "migration/MigratiorStatus.hpp"
#include "migration/TestMigrators.hpp"
#include "migration/impl/MigrationManagerBase.hpp"
#include "migration/impl/MigratorsRegister.hpp"
#include "util/MockMigrationBackend.hpp"
#include "util/MockMigrationBackendFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>

using TestMigratorRegister =
    migration::impl::MigratorsRegister<MockMigrationBackend, SimpleTestMigrator, SimpleTestMigrator2>;

using TestCassandraMigrationManager = migration::impl::MigrationManagerBase<TestMigratorRegister>;

struct MigrationManagerBaseTest : public util::prometheus::WithMockPrometheus, public MockMigrationBackendTestStrict {
    util::config::ClioConfigDefinition cfg{
        {"migration.full_scan_threads",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(2).withConstraint(
             util::config::gValidateUint32
         )}
    };
    std::shared_ptr<TestCassandraMigrationManager> migrationManager;

    MigrationManagerBaseTest()
    {
        auto mockBackendPtr = backend_.operator std::shared_ptr<MockMigrationBackend>();
        TestMigratorRegister const migratorRegister(mockBackendPtr);
        migrationManager = std::make_shared<TestCassandraMigrationManager>(mockBackendPtr, cfg.getObject("migration"));
    }
};

TEST_F(MigrationManagerBaseTest, AllStatus)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));
    auto const status = migrationManager->allMigratorsStatusPairs();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::ranges::find(status, std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::Migrated)) !=
        status.end()
    );
    EXPECT_TRUE(
        std::ranges::find(status, std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)) !=
        status.end()
    );
}

TEST_F(MigrationManagerBaseTest, AllNames)
{
    auto const names = migrationManager->allMigratorsNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "SimpleTestMigrator");
    EXPECT_EQ(names[1], "SimpleTestMigrator2");
}

TEST_F(MigrationManagerBaseTest, Description)
{
    EXPECT_EQ(migrationManager->getMigratorDescriptionByName("unknown"), "No Description");
    EXPECT_EQ(migrationManager->getMigratorDescriptionByName("SimpleTestMigrator"), "The migrator for version 0 -> 1");
    EXPECT_EQ(migrationManager->getMigratorDescriptionByName("SimpleTestMigrator2"), "The migrator for version 1 -> 2");
}

TEST_F(MigrationManagerBaseTest, RunMigration)
{
    EXPECT_CALL(*backend_, writeMigratorStatus("SimpleTestMigrator", "Migrated"));
    migrationManager->runMigration("SimpleTestMigrator");
}

TEST_F(MigrationManagerBaseTest, getMigratorStatusByName)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));

    EXPECT_EQ(migrationManager->getMigratorStatusByName("SimpleTestMigrator"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(migrationManager->getMigratorStatusByName("SimpleTestMigrator2"), migration::MigratorStatus::NotMigrated);
}
