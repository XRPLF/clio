#include "migration/impl/MigrationManagerFactory.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <gtest/gtest.h>

struct MigrationManagerFactoryTests : public virtual ::testing::Test {};

TEST_F(MigrationManagerFactoryTests, InvalidDBType)
{
    MockLedgerCache cache{};
    util::config::ClioConfigDefinition const configDef{
        {"database.type",
         util::config::ConfigValue{util::config::ConfigType::String}.defaultValue("invalid")}
    };
    auto const ret = migration::impl::makeMigrationManager(configDef, cache);
    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.error(), "Invalid database type");
}
