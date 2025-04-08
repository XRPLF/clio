//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "cluster/ClioNode.hpp"
#include "util/TimeUtils.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cluster;

struct ClioNodeTest : testing::Test {
    std::string const updateTimeStr = "2015-05-15T12:00:00Z";
    std::chrono::system_clock::time_point const updateTime =
        util::systemTpFromUtcStr(updateTimeStr, ClioNode::kTIME_FORMAT).value();
};

TEST_F(ClioNodeTest, Serialization)
{
    // Create a ClioNode with test data
    ClioNode const node{
        .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()()), .updateTime = updateTime
    };

    // Serialize to JSON
    boost::json::value jsonValue;
    EXPECT_NO_THROW(boost::json::value_from(node, jsonValue));

    // Verify JSON structure
    ASSERT_TRUE(jsonValue.is_object()) << jsonValue;
    auto const& obj = jsonValue.as_object();

    // Check update_time exists and is a string
    EXPECT_TRUE(obj.contains("update_time"));
    EXPECT_TRUE(obj.at("update_time").is_string());
}

TEST_F(ClioNodeTest, Deserialization)
{
    boost::json::value const jsonValue = {{"update_time", updateTimeStr}};

    // Deserialize to ClioNode
    ClioNode node{.uuid = std::make_shared<boost::uuids::uuid>(), .updateTime = {}};
    EXPECT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));

    // Verify deserialized data
    EXPECT_NE(node.uuid, nullptr);
    EXPECT_EQ(*node.uuid, boost::uuids::uuid{});
    EXPECT_EQ(node.updateTime, updateTime);
}

TEST_F(ClioNodeTest, DeserializationInvalidTime)
{
    // Prepare an invalid time format
    boost::json::value const jsonValue{"update_time", "invalid_format"};

    // Expect an exception during deserialization
    EXPECT_THROW(boost::json::value_to<ClioNode>(jsonValue), std::runtime_error);
}

TEST_F(ClioNodeTest, DeserializationMissingTime)
{
    // Prepare a JSON object without update_time
    boost::json::value const jsonValue = {{}};

    // Expect an exception
    EXPECT_THROW(boost::json::value_to<ClioNode>(jsonValue), std::runtime_error);
}
