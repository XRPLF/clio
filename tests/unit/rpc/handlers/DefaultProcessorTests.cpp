#include "rpc/FakesAndMocks.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "rpc/common/impl/Processors.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace std;

using namespace rpc;
using namespace rpc::validation;
using namespace tests::common;

class RPCDefaultProcessorTest : public HandlerBaseTest {};

TEST_F(RPCDefaultProcessorTest, ValidInput)
{
    runSpawn([](auto yield) {
        HandlerMock const handler;
        rpc::impl::DefaultProcessor<HandlerMock> const processor;

        auto const input = boost::json::parse(R"JSON({ "something": "works" })JSON");
        auto const spec = RpcSpec{{"something", Required{}}};
        auto const data = InOutFake{"works"};
        EXPECT_CALL(handler, spec(_)).WillOnce(ReturnRef(spec));
        EXPECT_CALL(handler, process(Eq(data), _)).WillOnce(Return(data));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);  // no error
        EXPECT_TRUE(ret.warnings.empty());
    });
}

TEST_F(RPCDefaultProcessorTest, NoInputValidCall)
{
    runSpawn([](auto yield) {
        HandlerWithoutInputMock const handler;
        rpc::impl::DefaultProcessor<HandlerWithoutInputMock> const processor;

        auto const data = InOutFake{"works"};
        auto const input = boost::json::parse(R"JSON({})JSON");
        EXPECT_CALL(handler, process(_)).WillOnce(Return(data));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);  // no error
        EXPECT_TRUE(ret.warnings.empty());
    });
}

TEST_F(RPCDefaultProcessorTest, InvalidInput)
{
    runSpawn([](auto yield) {
        HandlerMock const handler;
        rpc::impl::DefaultProcessor<HandlerMock> const processor;

        auto const input = boost::json::parse(R"JSON({ "other": "nope" })JSON");
        auto const spec = RpcSpec{{"something", Required{}}};
        EXPECT_CALL(handler, spec(_)).WillOnce(ReturnRef(spec));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_FALSE(ret);  // returns error
        EXPECT_TRUE(ret.warnings.empty());
    });
}
