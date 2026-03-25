#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Ping.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace rpc;

class RPCPingHandlerTest : public HandlerBaseTest {};

// example handler tests
TEST_F(RPCPingHandlerTest, Default)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{PingHandler{}};
        auto const output = handler.process(boost::json::parse(R"JSON({})JSON"), Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(R"JSON({})JSON"));
    });
}
