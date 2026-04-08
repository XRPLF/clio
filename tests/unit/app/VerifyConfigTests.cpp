#include "app/VerifyConfig.hpp"
#include "util/TmpFile.hpp"
#include "util/config/FakeConfigData.hpp"

#include <gtest/gtest.h>

using namespace app;
using namespace util::config;

TEST(VerifyConfigTest, InvalidConfig)
{
    auto const tmpConfigFile = TmpFile(kJSON_DATA);

    // false because json data(kJSON_DATA) is not compatible with current configDefinition
    EXPECT_FALSE(parseConfig(tmpConfigFile.path));
}

TEST(VerifyConfigTest, ValidConfig)
{
    // used to Verify Config test
    static constexpr auto kVALID_JSON_DATA = R"JSON({
        "server": {
            "ip": "0.0.0.0",
            "port": 51233
        }
    })JSON";
    auto const tmpConfigFile = TmpFile(kVALID_JSON_DATA);

    // current example config should always be compatible with configDefinition
    EXPECT_TRUE(parseConfig(tmpConfigFile.path));
}

TEST(VerifyConfigTest, ConfigFileNotExist)
{
    EXPECT_FALSE(parseConfig("doesn't exist Config File"));
}

TEST(VerifyConfigTest, InvalidJsonFile)
{
    // invalid json because extra "," after 51233
    static constexpr auto kINVALID_JSON = R"JSON({
        "server": {
            "ip": "0.0.0.0",
            "port": 51233,
        }
    })JSON";
    auto const tmpConfigFile = TmpFile(kINVALID_JSON);

    EXPECT_FALSE(parseConfig(tmpConfigFile.path));
}
