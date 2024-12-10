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
#include "util/config/Config.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

using EmptyMigratorRegister = migration::impl::MigratorsRegister<MockMigrationBackend>;

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
}

using MultipleMigratorRegister =
    migration::impl::MigratorsRegister<MockMigrationBackend, SimpleTestMigrator, SimpleTestMigrator2>;

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
    EXPECT_CALL(*backend, fetchMigratorStatus(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(std::nullopt));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenReturnInvalidStatus)
{
    EXPECT_CALL(*backend, fetchMigratorStatus(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return("Invalid"));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorsStatusWhenOneMigrated)
{
    EXPECT_CALL(*backend, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));

    auto const status = migratorRegister->getMigratorsStatus();
    EXPECT_EQ(status.size(), 2);
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator", migration::MigratorStatus::Migrated)
        ) != status.end()
    );
    EXPECT_TRUE(
        std::find(
            status.begin(), status.end(), std::make_tuple("SimpleTestMigrator2", migration::MigratorStatus::NotMigrated)
        ) != status.end()
    );
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatus)
{
    EXPECT_CALL(*backend, fetchMigratorStatus("SimpleTestMigrator", testing::_)).WillOnce(testing::Return("Migrated"));
    EXPECT_CALL(*backend, fetchMigratorStatus("SimpleTestMigrator2", testing::_))
        .WillOnce(testing::Return("NotMigrated"));

    EXPECT_EQ(migratorRegister->getMigratorStatus("unknown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(migratorRegister->getMigratorStatus("SimpleTestMigrator2"), migration::MigratorStatus::NotMigrated);
}

TEST_F(MultipleMigratorRegisterTests, GetMigratorStatusWhenError)
{
    EXPECT_CALL(*backend, fetchMigratorStatus(testing::_, testing::_))
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
    EXPECT_TRUE(std::find(names.begin(), names.end(), "SimpleTestMigrator") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "SimpleTestMigrator2") != names.end());
}

TEST_F(MultipleMigratorRegisterTests, RunUnknownMigrator)
{
    EXPECT_CALL(*backend, writeMigratorStatus(testing::_, testing::_)).Times(0);
    EXPECT_NO_THROW(migratorRegister->runMigrator("unknown", cfg));
}

TEST_F(MultipleMigratorRegisterTests, MigrateNormalMigrator)
{
    EXPECT_CALL(*backend, writeMigratorStatus("SimpleTestMigrator", "Migrated")).Times(1);
    EXPECT_NO_THROW(migratorRegister->runMigrator("SimpleTestMigrator", cfg));
}
