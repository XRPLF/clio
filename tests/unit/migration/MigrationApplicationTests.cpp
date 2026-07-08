#include "migration/MigrationApplication.hpp"
#include "migration/MigratiorStatus.hpp"
#include "util/MockMigrationManager.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace app;
using testing::Return;
using testing::StrictMock;

namespace {

constexpr auto kMigratorName = "TestMigrator";

}  // namespace

struct MigrationApplicationTest : util::prometheus::WithPrometheus {
    std::shared_ptr<StrictMock<MockMigrationManager>> manager =
        std::make_shared<StrictMock<MockMigrationManager>>();

    [[nodiscard]] MigratorApplication
    makeApp(MigrateSubCmd command) const
    {
        return MigratorApplication{std::move(command), manager};
    }
};

// A status command lists every registered migrator with its description and status.
TEST_F(MigrationApplicationTest, StatusListsAllMigrators)
{
    EXPECT_CALL(*manager, allMigratorsStatusPairs())
        .WillOnce(Return(
            std::vector<std::tuple<std::string, migration::MigratorStatus>>{
                {kMigratorName, migration::MigratorStatus::Status::Migrated}
            }
        ));
    EXPECT_CALL(*manager, getMigratorDescriptionByName(kMigratorName))
        .WillOnce(Return("A test migrator"));

    auto app = makeApp(MigrateSubCmd::status());
    EXPECT_EQ(app.run(), EXIT_SUCCESS);
}

// A status command with no registered migrators still succeeds.
TEST_F(MigrationApplicationTest, StatusWithNoMigrators)
{
    EXPECT_CALL(*manager, allMigratorsStatusPairs())
        .WillOnce(Return(std::vector<std::tuple<std::string, migration::MigratorStatus>>{}));

    auto app = makeApp(MigrateSubCmd::status());
    EXPECT_EQ(app.run(), EXIT_SUCCESS);
}

// Running an already-migrated migrator is a no-op that reports success and prints the status.
TEST_F(MigrationApplicationTest, MigrateAlreadyMigrated)
{
    EXPECT_CALL(*manager, getMigratorStatusByName(kMigratorName))
        .WillOnce(Return(migration::MigratorStatus::Status::Migrated));
    EXPECT_CALL(*manager, allMigratorsStatusPairs())
        .WillOnce(Return(std::vector<std::tuple<std::string, migration::MigratorStatus>>{}));

    auto app = makeApp(MigrateSubCmd::migration(kMigratorName));
    EXPECT_EQ(app.run(), EXIT_SUCCESS);
}

// Running an unknown migrator reports failure and prints the status.
TEST_F(MigrationApplicationTest, MigrateUnknownMigrator)
{
    EXPECT_CALL(*manager, getMigratorStatusByName(kMigratorName))
        .WillOnce(Return(migration::MigratorStatus::Status::NotKnown));
    EXPECT_CALL(*manager, allMigratorsStatusPairs())
        .WillOnce(Return(std::vector<std::tuple<std::string, migration::MigratorStatus>>{}));

    auto app = makeApp(MigrateSubCmd::migration(kMigratorName));
    EXPECT_EQ(app.run(), EXIT_FAILURE);
}

// Running a not-yet-migrated migrator triggers the migration and reports success.
TEST_F(MigrationApplicationTest, MigrateRunsMigration)
{
    EXPECT_CALL(*manager, getMigratorStatusByName(kMigratorName))
        .WillOnce(Return(migration::MigratorStatus::Status::NotMigrated));
    EXPECT_CALL(*manager, runMigration(kMigratorName));

    auto app = makeApp(MigrateSubCmd::migration(kMigratorName));
    EXPECT_EQ(app.run(), EXIT_SUCCESS);
}
