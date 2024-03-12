//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "etl/ETLState.hpp"
#include "util/Fixtures.hpp"
#include "util/MockSource.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

namespace json = boost::json;
using namespace util;
using namespace testing;

struct ETLStateTest : public NoLoggerFixture {
    MockSource source = MockSource{};
};

TEST_F(ETLStateTest, Error)
{
    EXPECT_CALL(source, forwardToRippled).WillOnce(Return(std::nullopt));
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
    ASSERT_TRUE(state->networkID.has_value());
    EXPECT_EQ(state->networkID.value(), 12);
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
    EXPECT_FALSE(state->networkID.has_value());
}
