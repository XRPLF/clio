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

#include <gtest/gtest.h>

TEST(MigratiorStatus, ToString)
{
    migration::MigratorStatus status(migration::MigratorStatus::Migrated);
    EXPECT_EQ(status.toString(), "Migrated");
    status = migration::MigratorStatus(migration::MigratorStatus::NotMigrated);
    EXPECT_EQ(status.toString(), "NotMigrated");
    status = migration::MigratorStatus(migration::MigratorStatus::NotKnown);
    EXPECT_EQ(status.toString(), "NotKnown");
}

TEST(MigratiorStatus, FromString)
{
    EXPECT_EQ(migration::MigratorStatus::fromString("Migrated"), migration::MigratorStatus::Migrated);
    EXPECT_EQ(migration::MigratorStatus::fromString("NotMigrated"), migration::MigratorStatus::NotMigrated);
    EXPECT_EQ(migration::MigratorStatus::fromString("NotKnown"), migration::MigratorStatus::NotKnown);
    EXPECT_EQ(migration::MigratorStatus::fromString("Unknown"), migration::MigratorStatus::NotMigrated);
}

TEST(MigratiorStatus, Compare)
{
    migration::MigratorStatus const status1(migration::MigratorStatus::Migrated);
    migration::MigratorStatus status2(migration::MigratorStatus::Migrated);
    EXPECT_TRUE(status1 == status2);
    status2 = migration::MigratorStatus(migration::MigratorStatus::NotMigrated);
    EXPECT_FALSE(status1 == status2);
    EXPECT_FALSE(status1 == migration::MigratorStatus::NotMigrated);
    EXPECT_TRUE(status1 == migration::MigratorStatus::Migrated);
}
