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
#include <optional>
#include <string>
#include <tuple>

using EmptyMigratorRegister = migration::impl::MigratorsRegister<MockMigrationBackend>;

namespace {
util::config::ClioConfigDefinition cfg{
    {{"migration.full_scan_threads",
      util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(2).withConstraint(
          util::config::validateUint32
      )},
     {"migration.full_scan_jobs",
      util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(4).withConstraint(
          util::config::validateUint32
      )},
     {"migration.cursors_per_job",
      util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(100).withConstraint(
          util::config::validateUint32
      )}}
};
}  // namespace

struct MigratorRegisterTests : public util::prometheus::WithMockPrometheus, public MockMigrationBackendTest {};

TEST_F(MigratorRegisterTests, EmptyMigratorRegister)
{
    EmptyMigratorRegister migratorRegister(backend_);
    EXPECT_EQ(migratorRegister.getMigratorsStatus().size(), 0);
    EXPECT_EQ(migratorRegister.getMigratorNames().size(), 0);
    EXPECT_EQ(migratorRegister.getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_NO_THROW(migratorRegister.runMigrator("unknown", cfg.getObject("migration")));
    EXPECT_EQ(migratorRegister.getMigratorDescription("unknown"), "No Description");
}

using MultipleMigratorRegister =
    migration::impl::MigratorsRegister<MockMigrationBackend, SimpleTestMigrator, SimpleTestMigrator2>;

struct MultipleMigratorRegisterTests : public util::prometheus::WithMockPrometheus, public MockMigrationBackendTest {
    std::optional<MultipleMigratorRegister> migratorRegister;

    MultipleMigratorRegisterTests()
    {
        migratorRegister.emplace(backend_);
    }
};

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenError)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(std::nullopt));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenReturnInvalidStatus)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return("Invalid"));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenOneMigrated)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::Migrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::ranges::find(
            status, std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatus)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));

    EXPECT_EQ(migratorRegister->getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator2"), migration::MigratorStatus::NotMigrated);
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatusWhenError)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_EQ(migratorRegister->getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator"), migration::MigratorStatus::NotMigrated);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator2"), migration::MigratorStatus::NotMigrated);
}

TEST_F(MultipleMigratorRegisterTests, Names)
{
    auto names = migratorRegister->getMigratorNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(std::ranges::find(names, "SimpleTestMigrator") != names.end());
    EXPECT_TRUE(std::ranges::find(names, "SimpleTestMigrator2") != names.end());
}

TEST_F(MultipleMigratorRegisterTests, Description)
{
    EXPECT_EQ(migratorRegister->getMigratorDescription("unknown"), "No Description");
    EXPECT_EQ(migratorRegister->getMigratorDescription("SimpleTestMigrator"), "The migrator for version 0 -> 1");
    EXPECT_EQ(migratorRegister->getMigratorDescription("SimpleTestMigrator2"), "The migrator for version 1 -> 2");
}

TEST_F(MultipleMigratorRegisterTests, RunUnknownMigrator)
{
    EXPECT_CALL(*backend_, writeMigratorStatus(testing::_, testing::_)).Times(0);
    EXPECT_NO_THROW(migratorRegister->runMigrator("unknown", cfg.getObject("migration")));
}

TEST_F(MultipleMigratorRegisterTests, MigrateNormalMigrator)
{
    EXPECT_CALL(*backend_, writeMigratorStatus("SimpleTestMigrator", "Migrated")).Times(1);
    EXPECT_NO_THROW(migratorRegister->runMigrator("SimpleTestMigrator", cfg.getObject("migration")));
}
