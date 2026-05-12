#include "migration/MigratiorStatus.hpp"

#include <gtest/gtest.h>

TEST(MigratiorStatus, ToString)
{
    migration::MigratorStatus status(migration::MigratorStatus::Status::Migrated);
    EXPECT_EQ(status.toString(), "Migrated");
    status = migration::MigratorStatus(migration::MigratorStatus::Status::NotMigrated);
    EXPECT_EQ(status.toString(), "NotMigrated");
    status = migration::MigratorStatus(migration::MigratorStatus::Status::NotKnown);
    EXPECT_EQ(status.toString(), "NotKnown");
}

TEST(MigratiorStatus, FromString)
{
    EXPECT_EQ(
        migration::MigratorStatus::fromString("Migrated"),
        migration::MigratorStatus::Status::Migrated
    );
    EXPECT_EQ(
        migration::MigratorStatus::fromString("NotMigrated"),
        migration::MigratorStatus::Status::NotMigrated
    );
    EXPECT_EQ(
        migration::MigratorStatus::fromString("NotKnown"),
        migration::MigratorStatus::Status::NotKnown
    );
    EXPECT_EQ(
        migration::MigratorStatus::fromString("Unknown"),
        migration::MigratorStatus::Status::NotMigrated
    );
}

TEST(MigratiorStatus, Compare)
{
    migration::MigratorStatus const status1(migration::MigratorStatus::Status::Migrated);
    migration::MigratorStatus status2(migration::MigratorStatus::Status::Migrated);
    EXPECT_TRUE(status1 == status2);
    status2 = migration::MigratorStatus(migration::MigratorStatus::Status::NotMigrated);
    EXPECT_FALSE(status1 == status2);
    EXPECT_FALSE(status1 == migration::MigratorStatus::Status::NotMigrated);
    EXPECT_TRUE(status1 == migration::MigratorStatus::Status::Migrated);
}
