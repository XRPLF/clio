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
#include "util/MockMigrationBackend.hpp"
#include "util/MockMigrationBackendFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/Config.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>

using EmptyMigratorRegister = migration::MigratorsRegister<MockMigrationBackend>;

struct MigratorRegisterTests : public util::prometheus::WithMockPrometheus, public MockMigrationBackendTest {
    util::Config cfg;
};

TEST_F(MigratorRegisterTests, EmptyMigratorRegister)
{
    auto mockBackendPtr = backend.operator std::shared_ptr<MockMigrationBackend>();
    EmptyMigratorRegister migratorRegister(mockBackendPtr);
    EXPECT_EQ(migratorRegister.getMigratorsStatus().size(), 0);
    EXPECT_EQ(migratorRegister.getMigratorNames().size(), 0);
    EXPECT_EQ(migratorRegister.getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_NO_THROW(migratorRegister.runMigrator("unknown", cfg));
    EXPECT_NO_THROW(migratorRegister.runRollback("unknown"));
}

using MultipleMigratorRegister =
    migration::MigratorsRegister<MockMigrationBackend, SimpleTestMigrator, RollbackableTestMigrator>;

struct MultipleMigratorRegisterTests : public util::prometheus::WithMockPrometheus, public MockMigrationBackendTest {
    util::Config cfg;

    std::optional<MultipleMigratorRegister> migratorRegister;

    MultipleMigratorRegisterTests()
    {
        auto mockBackendPtr = backend.operator std::shared_ptr<MockMigrationBackend>();
        migratorRegister.emplace(mockBackendPtr);
    }
};

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenError)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_)).WillOnce(testing::Return(std::nullopt));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
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

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenNothingMigrated)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_))
        .WillOnce(testing::Return(std::unordered_set<std::string>{}));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
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

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenOneMigrated)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_))
        .WillOnce(testing::Return(std::unordered_set<std::string>{"SimpleTestMigrator"}));

    auto const status = migratorRegister->getMigratorsStatus();
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

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatus)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_))
        .Times(3)
        .WillRepeatedly(testing::Return(std::unordered_set<std::string>{"SimpleTestMigrator"}));

    EXPECT_EQ(migratorRegister->getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(migratorRegister->getMigratorStatus("RollbackableTestMigrator"), migration::MigratorStatus::NotMigrated);
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatusWhenError)
{
    EXPECT_CALL(*backend, fetchMigratedFeatures(testing::_)).Times(3).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_EQ(migratorRegister->getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator"), migration::MigratorStatus::NotMigrated);
    EXPECT_EQ(migratorRegister->getMigratorStatus("RollbackableTestMigrator"), migration::MigratorStatus::NotMigrated);
}

TEST_F(MultipleMigratorRegisterTests, Names)
{
    auto names = migratorRegister->getMigratorNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "SimpleTestMigrator") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "RollbackableTestMigrator") != names.end());
}

TEST_F(MultipleMigratorRegisterTests, RollBackUnknownMigrator)
{
    EXPECT_CALL(*backend, removeMigratedMigrator(testing::_)).Times(0);
    EXPECT_NO_THROW(migratorRegister->runRollback("unknown"));
}

TEST_F(MultipleMigratorRegisterTests, RunUnknownMigrator)
{
    EXPECT_CALL(*backend, writeMigratedMigrator(testing::_)).Times(0);
    EXPECT_NO_THROW(migratorRegister->runMigrator("unknown", cfg));
}

TEST_F(MultipleMigratorRegisterTests, RollBackUnrollbackableMigrator)
{
    EXPECT_CALL(*backend, removeMigratedMigrator("SimpleTestMigrator")).Times(1);
    EXPECT_NO_THROW(migratorRegister->runRollback("SimpleTestMigrator"));
}

TEST_F(MultipleMigratorRegisterTests, RollBackRollbackableMigrator)
{
    EXPECT_CALL(*backend, removeMigratedMigrator("RollbackableTestMigrator")).Times(1);
    EXPECT_NO_THROW(migratorRegister->runRollback("RollbackableTestMigrator"));
}

TEST_F(MultipleMigratorRegisterTests, MigrateNormalMigrator)
{
    EXPECT_CALL(*backend, writeMigratedMigrator("SimpleTestMigrator")).Times(1);
    EXPECT_NO_THROW(migratorRegister->runMigrator("SimpleTestMigrator", cfg));
}
