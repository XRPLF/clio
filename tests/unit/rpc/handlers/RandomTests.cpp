#include "rpc/JS.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Random.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>
#include <xrpl/protocol/jss.h>

using namespace rpc;

class RPCRandomHandlerTest : public HandlerBaseTest {};

TEST_F(RPCRandomHandlerTest, Default)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{RandomHandler{}};
        auto const output = handler.process(boost::json::parse(R"JSON({})JSON"), Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains(JS(random)));
        EXPECT_EQ(output.result->as_object().at(JS(random)).as_string().size(), 64u);
    });
}
