#include "etl/ETLState.hpp"
#include "rpc/Errors.hpp"
#include "util/MockSource.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

namespace json = boost::json;
using namespace util;
using namespace testing;

struct ETLStateTest : public virtual ::testing::Test {
    MockSource source = MockSource{};
};

TEST_F(ETLStateTest, Error)
{
    EXPECT_CALL(source, forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlInvalidResponse}));
    auto const state = etl::ETLState::fetchETLStateFromSource(source);
    EXPECT_FALSE(state);
}

TEST_F(ETLStateTest, NetworkIdValid)
{
    auto const json = json::parse(
        R"JSON({
            "result": {
                "info": {
                    "network_id": 12
                }
            }
        })JSON"
    );
    EXPECT_CALL(source, forwardToRippled).WillOnce(Return(json.as_object()));
    auto const state = etl::ETLState::fetchETLStateFromSource(source);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->networkID, 12);
}

TEST_F(ETLStateTest, NetworkIdInvalid)
{
    auto const json = json::parse(
        R"JSON({
            "result": {
                "info": {
                    "network_id2": 12
                }
            }
        })JSON"
    );
    EXPECT_CALL(source, forwardToRippled).WillOnce(Return(json.as_object()));
    auto const state = etl::ETLState::fetchETLStateFromSource(source);
    ASSERT_TRUE(state.has_value());
    EXPECT_NE(state->networkID, 12);
}

TEST_F(ETLStateTest, ResponseHasError)
{
    auto const json = json::parse(
        R"JSON({
            "error": "error"
        })JSON"
    );
    EXPECT_CALL(source, forwardToRippled).WillOnce(Return(json.as_object()));
    auto const state = etl::ETLState::fetchETLStateFromSource(source);
    EXPECT_FALSE(state.has_value());
}
