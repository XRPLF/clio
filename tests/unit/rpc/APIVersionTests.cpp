#include "rpc/common/impl/APIVersionParser.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

namespace {

constexpr auto kDEFAULT_API_VERSION = 5u;
constexpr auto kMIN_API_VERSION = 2u;
constexpr auto kMAX_API_VERSION = 10u;

}  // namespace

using namespace util::config;
using namespace rpc::impl;
namespace json = boost::json;

class RPCAPIVersionTest : public virtual ::testing::Test {
protected:
    ProductionAPIVersionParser parser_{kDEFAULT_API_VERSION, kMIN_API_VERSION, kMAX_API_VERSION};
};

TEST_F(RPCAPIVersionTest, ReturnsDefaultVersionIfNotSpecified)
{
    auto ver = parser_.parse(json::parse("{}").as_object());
    EXPECT_TRUE(ver);
    EXPECT_EQ(ver.value(), kDEFAULT_API_VERSION);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionHigherThanMaxSupported)
{
    auto ver = parser_.parse(json::parse(R"JSON({"api_version": 11})JSON").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionLowerThanMinSupported)
{
    auto ver = parser_.parse(json::parse(R"JSON({"api_version": 1})JSON").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorOnWrongType)
{
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": null})JSON").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": "5"})JSON").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": "wrong"})JSON").as_object());
        EXPECT_FALSE(ver);
    }
}

TEST_F(RPCAPIVersionTest, ReturnsParsedVersionIfAllPreconditionsAreMet)
{
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": 2})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": 10})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = parser_.parse(json::parse(R"JSON({"api_version": 5})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
}

TEST_F(RPCAPIVersionTest, GetsValuesFromConfigCorrectly)
{
    ClioConfigDefinition const cfg{
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(kMIN_API_VERSION)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(kMAX_API_VERSION)},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(kDEFAULT_API_VERSION)}
    };

    ProductionAPIVersionParser const configuredParser{cfg.getObject("api_version")};

    {
        auto ver = configuredParser.parse(json::parse(R"JSON({"api_version": 2})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver =
            configuredParser.parse(json::parse(R"JSON({"api_version": 10})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"JSON({"api_version": 5})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"JSON({})JSON").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), kDEFAULT_API_VERSION);
    }
    {
        auto ver =
            configuredParser.parse(json::parse(R"JSON({"api_version": 11})JSON").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"JSON({"api_version": 1})JSON").as_object());
        EXPECT_FALSE(ver);
    }
}
