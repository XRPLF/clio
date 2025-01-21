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

#include "data/Types.hpp"
#include "migration/MigrationInspectorFactory.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

using namespace testing;

struct MigrationInspectorFactoryTests : public util::prometheus::WithPrometheus, public MockBackendTest {
protected:
    util::config::ClioConfigDefinition const readerConfig_ = util::config::ClioConfigDefinition{
        {"read_only", util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(true)}
    };
};

struct MigrationInspectorFactoryTestsDeathTest : public MigrationInspectorFactoryTests {};

TEST_F(MigrationInspectorFactoryTestsDeathTest, NullBackend)
{
    EXPECT_DEATH(migration::makeMigrationInspector(readerConfig_, nullptr), ".*");
}

TEST_F(MigrationInspectorFactoryTests, NotInitMigrationTableIfReader)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange).Times(0);

    EXPECT_NE(migration::makeMigrationInspector(readerConfig_, backend_), nullptr);
}

TEST_F(MigrationInspectorFactoryTests, BackendIsWriterAndDBEmpty)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(std::nullopt));

    util::config::ClioConfigDefinition const writerConfig = util::config::ClioConfigDefinition{
        {"read_only", util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(false)}
    };
    EXPECT_NE(migration::makeMigrationInspector(writerConfig, backend_), nullptr);
}

TEST_F(MigrationInspectorFactoryTests, BackendIsWriterAndDBNotEmpty)
{
    LedgerRange const range{.minSequence = 1, .maxSequence = 5};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(range));

    util::config::ClioConfigDefinition const writerConfig = util::config::ClioConfigDefinition{
        {"read_only", util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(false)}
    };
    EXPECT_NE(migration::makeMigrationInspector(writerConfig, backend_), nullptr);
}
