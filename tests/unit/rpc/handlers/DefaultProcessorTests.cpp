#include "rpc/Errors.hpp"
#include "rpc/FakesAndMocks.hpp"
#include "rpc/common/Concepts.hpp"
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

static_assert(SomeHandlerWithTypedInput<TypedHandlerFake>);
static_assert(not SomeHandlerWithInput<TypedHandlerFake>);
static_assert(SomeHandlerWithTypedInput<FailingTypedHandlerFake>);
static_assert(SomeHandlerWithInput<HandlerMock>);
static_assert(not SomeHandlerWithTypedInput<HandlerMock>);

// The four tests below exercise the typed path — a handler whose spec, validation and
// deserialization all come from the shared consteval spec via HandlerFor<Input>. They run
// against the same DefaultProcessor as the legacy tests above, which is the point: the
// dual path is a dispatch detail, not a second processor.

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_HappyPath)
{
    runSpawn([](auto yield) {
        TypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<TypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "hello": "world", "limit": 42 })JSON");

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);
        EXPECT_TRUE(ret.warnings.empty());
        EXPECT_EQ(ret.result.value().at("computed").as_string(), "world_42");
    });
}

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_MissingRequiredField_ReturnsError)
{
    runSpawn([](auto yield) {
        TypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<TypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "limit": 42 })JSON");

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_FALSE(ret);
        EXPECT_TRUE(ret.warnings.empty());
    });
}

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_DeprecatedField_WarningsForwarded)
{
    runSpawn([](auto yield) {
        TypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<TypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "hello": "world", "old_field": true })JSON");

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);
        EXPECT_EQ(ret.warnings.size(), 1);
    });
}

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_HandlerReturnsError_ForwardsError)
{
    runSpawn([](auto yield) {
        FailingTypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<FailingTypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "hello": "world", "limit": 42 })JSON");
        auto const ret = processor(handler, input, Context{yield});

        ASSERT_FALSE(ret);
        EXPECT_EQ(rpc::makeError(ret.result.error()).at("error").as_string(), "Very custom error");
        EXPECT_TRUE(ret.warnings.empty());
    });
}

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_HandlerReturnsError_StillForwardsWarnings)
{
    runSpawn([](auto yield) {
        FailingTypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<FailingTypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "hello": "world", "old_field": true })JSON");
        auto const ret = processor(handler, input, Context{yield});

        ASSERT_FALSE(ret);
        EXPECT_EQ(rpc::makeError(ret.result.error()).at("error").as_string(), "Very custom error");
        EXPECT_EQ(ret.warnings.size(), 1);
    });
}

TEST_F(RPCDefaultProcessorTest, NewSpecHandler_DeprecatedFieldAbsent_NoWarnings)
{
    runSpawn([](auto yield) {
        TypedHandlerFake const handler;
        rpc::impl::DefaultProcessor<TypedHandlerFake> const processor;

        auto const input = boost::json::parse(R"JSON({ "hello": "world" })JSON");

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);
        EXPECT_TRUE(ret.warnings.empty());
    });
}
