//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "migration/MigratiorStatus.hpp"
#include "migration/TestMigrators.hpp"
#include "migration/impl/MigrationManagerBase.hpp"
#include "migration/impl/MigratorsRegister.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>

using TestMigratorRegister =
    migration::impl::MigratorsRegister<data::BackendInterface, SimpleTestMigrator, SimpleTestMigrator2>;

using TestCassandramigrationInspector = migration::impl::MigrationInspectorBase<TestMigratorRegister>;

struct MigrationInspectorBaseTest : public util::prometheus::WithMockPrometheus, public MockBackendTest {
    MigrationInspectorBaseTest()
    {
        migrationInspector_ = std::make_shared<TestCassandramigrationInspector>(backend_);
    }

protected:
    std::shared_ptr<TestCassandramigrationInspector> migrationInspector_;
};

TEST_F(MigrationInspectorBaseTest, AllStatus)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));
    auto const status = migrationInspector_->allMigratorsStatusPairs();
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

TEST_F(MigrationInspectorBaseTest, AllNames)
{
    auto const names = migrationInspector_->allMigratorsNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "SimpleTestMigrator");
    EXPECT_EQ(names[1], "SimpleTestMigrator2");
}

TEST_F(MigrationInspectorBaseTest, Description)
{
    EXPECT_EQ(migrationInspector_->getMigratorDescriptionByName("unknown"), "No Description");
    EXPECT_EQ(
        migrationInspector_->getMigratorDescriptionByName("SimpleTestMigrator"), "The migrator for version 0 -> 1"
    );
    EXPECT_EQ(
        migrationInspector_->getMigratorDescriptionByName("SimpleTestMigrator2"), "The migrator for version 1 -> 2"
    );
}

TEST_F(MigrationInspectorBaseTest, getMigratorStatusByName)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_EQ(migrationInspector_->getMigratorStatusByName("SimpleTestMigrator"), migration::MigratorStatus::Migrated);

    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));
    EXPECT_EQ(
        migrationInspector_->getMigratorStatusByName("SimpleTestMigrator2"), migration::MigratorStatus::NotMigrated
    );
}

TEST_F(MigrationInspectorBaseTest, oneMigratorBlockingClio)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_))
        .WillOnce(testing::Return("NotMigrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_)).Times(0);

    EXPECT_TRUE(migrationInspector_->isBlockingClio());
}

TEST_F(MigrationInspectorBaseTest, oneMigratorBlockingClioGetMigrated)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend_, fetchMigratorStatus("SimpleTestMigrator2", testing::_)).Times(0);
    EXPECT_FALSE(migrationInspector_->isBlockingClio());
}

TEST_F(MigrationInspectorBaseTest, noMigratorBlockingClio)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus).Times(0);

    auto const migrations = migration::impl::MigrationInspectorBase<
        migration::impl::MigratorsRegister<data::BackendInterface, SimpleTestMigrator2, SimpleTestMigrator3>>(backend_);
    EXPECT_FALSE(migrations.isBlockingClio());
}

TEST_F(MigrationInspectorBaseTest, isBlockingClioWhenNoMigrator)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus).Times(0);

    auto const migrations =
        migration::impl::MigrationInspectorBase<migration::impl::MigratorsRegister<data::BackendInterface>>(backend_);
    EXPECT_FALSE(migrations.isBlockingClio());
}
