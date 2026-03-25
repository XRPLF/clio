#include "data/Types.hpp"
#include "migration/MigrationInspectorFactory.hpp"
#include "util/MockAssert.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

using namespace testing;
using namespace data;

struct MigrationInspectorFactoryTests : util::prometheus::WithPrometheus,
                                        common::util::WithMockAssert,
                                        MockBackendTest {
protected:
    util::config::ClioConfigDefinition const readerConfig_ = util::config::ClioConfigDefinition{
        {"read_only",
         util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(true)}
    };
};

TEST_F(MigrationInspectorFactoryTests, NullBackend)
{
    EXPECT_CLIO_ASSERT_FAIL(migration::makeMigrationInspector(readerConfig_, nullptr));
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
        {"read_only",
         util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(false)}
    };
    EXPECT_NE(migration::makeMigrationInspector(writerConfig, backend_), nullptr);
}

TEST_F(MigrationInspectorFactoryTests, BackendIsWriterAndDBNotEmpty)
{
    LedgerRange const range{.minSequence = 1, .maxSequence = 5};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(range));

    util::config::ClioConfigDefinition const writerConfig = util::config::ClioConfigDefinition{
        {"read_only",
         util::config::ConfigValue{util::config::ConfigType::Boolean}.defaultValue(false)}
    };
    EXPECT_NE(migration::makeMigrationInspector(writerConfig, backend_), nullptr);
}
