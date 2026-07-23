#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/VersionHandler.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

namespace {

constexpr auto kDefaultApiVersion = 3u;
constexpr auto kMinApiVersion = 2u;
constexpr auto kMaxApiVersion = 10u;

}  // namespace

using namespace rpc;
using namespace util::config;

class RPCVersionHandlerTest : public HandlerBaseTest {};

TEST_F(RPCVersionHandlerTest, Default)
{
    ClioConfigDefinition cfg{
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(kMinApiVersion)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(kMaxApiVersion)},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(kDefaultApiVersion)}
    };

    boost::json::value jsonData = boost::json::parse(
        fmt::format(
            R"JSON({{
                "api_version.min": {},
                "api_version.max": {},
                "api_version.default": {}
            }})JSON",
            kMinApiVersion,
            kMaxApiVersion,
            kDefaultApiVersion
        )
    );

    runSpawn([&](auto yield) {
        auto const handler = AnyHandler{VersionHandler{cfg}};
        auto const output = handler.process(jsonData, Context{yield});
        ASSERT_TRUE(output);

        // check all against all the correct values
        auto const& result = output.result.value().as_object();
        EXPECT_TRUE(result.contains("version"));
        auto const& info = result.at("version").as_object();
        EXPECT_TRUE(info.contains("first"));
        EXPECT_TRUE(info.contains("last"));
        EXPECT_TRUE(info.contains("good"));
        EXPECT_EQ(info.at("first"), 2u);
        EXPECT_EQ(info.at("last"), 10u);
        EXPECT_EQ(info.at("good"), 3u);
    });
}
