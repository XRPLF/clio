#include "rpc/Errors.hpp"
#include "rpc/FakesAndMocks.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace rpc;
using namespace rpc::validation;
using namespace tests::common;

namespace json = boost::json;

class RPCTestHandlerTest : public HandlerBaseTest {};

// example handler tests
TEST_F(RPCTestHandlerTest, HandlerSuccess)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{HandlerFake{}};
        auto const input = json::parse(R"JSON({
            "hello": "world",
            "limit": 10
        })JSON");

        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const val = output.result.value();
        EXPECT_EQ(val.as_object().at("computed").as_string(), "world_10");
    });
}

TEST_F(RPCTestHandlerTest, NoInputHandlerSuccess)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{NoInputHandlerFake{}};
        auto const output = handler.process(json::parse(R"JSON({})JSON"), Context{yield});
        ASSERT_TRUE(output);

        auto const val = output.result.value();
        EXPECT_EQ(val.as_object().at("computed").as_string(), "test");
    });
}

TEST_F(RPCTestHandlerTest, HandlerErrorHandling)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{HandlerFake{}};
        auto const input = json::parse(R"JSON({
            "hello": "not world",
            "limit": 10
        })JSON");

        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_code").as_uint64(), rpc::RippledError::rpcINVALID_PARAMS);
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTestHandlerTest, HandlerInnerErrorHandling)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{FailingHandlerFake{}};
        auto const input = json::parse(R"JSON({
            "hello": "world",
            "limit": 10
        })JSON");

        // validation succeeds but handler itself returns error
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "Very custom error");
    });
}
