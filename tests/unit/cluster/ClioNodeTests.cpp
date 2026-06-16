#include "cluster/ClioNode.hpp"
#include "util/MockLedgerCacheLoadingState.hpp"
#include "util/MockWriterState.hpp"
#include "util/NameGenerator.hpp"
#include "util/TimeUtils.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cluster;

struct ClioNodeTest : testing::Test {
    std::string const updateTimeStr = "2015-05-15T12:00:00Z";
    std::chrono::system_clock::time_point const updateTime =
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        *util::systemTpFromUtcStr(updateTimeStr, ClioNode::kTimeFormat);
};

TEST_F(ClioNodeTest, Serialization)
{
    ClioNode const node{
        .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()()),
        .updateTime = updateTime,
        .dbRole = ClioNode::DbRole::Writer,
        .etlStarted = true,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = true
    };

    boost::json::value jsonValue;
    EXPECT_NO_THROW(boost::json::value_from(node, jsonValue));

    ASSERT_TRUE(jsonValue.is_object()) << jsonValue;
    auto const& obj = jsonValue.as_object();

    EXPECT_TRUE(obj.contains("update_time"));
    EXPECT_TRUE(obj.at("update_time").is_string());

    EXPECT_TRUE(obj.contains("db_role"));
    EXPECT_TRUE(obj.at("db_role").is_number());
    EXPECT_EQ(obj.at("db_role").as_int64(), static_cast<int64_t>(node.dbRole));

    EXPECT_TRUE(obj.contains("etl_started"));
    EXPECT_EQ(obj.at("etl_started").as_bool(), node.etlStarted);

    EXPECT_TRUE(obj.contains("cache_is_full"));
    EXPECT_EQ(obj.at("cache_is_full").as_bool(), node.cacheIsFull);

    EXPECT_TRUE(obj.contains("cache_is_currently_loading"));
    EXPECT_EQ(obj.at("cache_is_currently_loading").as_bool(), node.cacheIsCurrentlyLoading);
}

TEST_F(ClioNodeTest, Deserialization)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", 1},
        {"etl_started", true},
        {"cache_is_full", false},
        {"cache_is_currently_loading", true}
    };

    ClioNode node{
        .uuid = std::make_shared<boost::uuids::uuid>(),
        .updateTime = {},
        .dbRole = ClioNode::DbRole::ReadOnly,
        .etlStarted = false,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = false
    };
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));

    EXPECT_NE(node.uuid, nullptr);
    EXPECT_EQ(*node.uuid, boost::uuids::uuid{});
    EXPECT_EQ(node.updateTime, updateTime);
    EXPECT_EQ(node.dbRole, ClioNode::DbRole::NotWriter);
    EXPECT_TRUE(node.etlStarted);
    EXPECT_FALSE(node.cacheIsFull);
    EXPECT_TRUE(node.cacheIsCurrentlyLoading);
}

TEST_F(ClioNodeTest, DeserializationInvalidTime)
{
    boost::json::value const jsonValue{"update_time", "invalid_format"};
    EXPECT_THROW(boost::json::value_to<ClioNode>(jsonValue), std::runtime_error);
}

TEST_F(ClioNodeTest, DeserializationMissingTime)
{
    // Prepare a JSON object without update_time
    boost::json::value const jsonValue = {{}};

    // Expect an exception
    EXPECT_THROW(boost::json::value_to<ClioNode>(jsonValue), std::runtime_error);
}

TEST_F(ClioNodeTest, DeserializationMissingEtlStarted)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", 1},
        {"cache_is_full", false},
        {"cache_is_currently_loading", false}
    };
    ClioNode node{};
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));
    EXPECT_TRUE(node.etlStarted);  // defaults to true
}

TEST_F(ClioNodeTest, DeserializationMissingCacheIsFull)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", 1},
        {"etl_started", true},
        {"cache_is_currently_loading", false}
    };
    ClioNode node{};
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));
    EXPECT_TRUE(node.cacheIsFull);  // defaults to true
}

TEST_F(ClioNodeTest, DeserializationMissingCacheIsCurrentlyLoading)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", 1},
        {"etl_started", true},
        {"cache_is_full", false}
    };
    ClioNode node{};
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));
    EXPECT_FALSE(node.cacheIsCurrentlyLoading);  // defaults to false
}

TEST_F(ClioNodeTest, DeserializationMissingDbRole)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"etl_started", false},
        {"cache_is_full", false},
        {"cache_is_currently_loading", false}
    };
    ClioNode node{};
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));
    EXPECT_EQ(node.dbRole, ClioNode::DbRole::Fallback);  // defaults to Fallback
}

TEST_F(ClioNodeTest, DeserializationOldNodeFormat)
{
    // Old nodes (pre cluster-coordination release) only write update_time.
    // Parsing must succeed with safe backward-compatible defaults.
    boost::json::value const jsonValue = {{"update_time", updateTimeStr}};
    ClioNode node{};
    ASSERT_NO_THROW(node = boost::json::value_to<ClioNode>(jsonValue));
    EXPECT_EQ(node.updateTime, updateTime);
    EXPECT_EQ(node.dbRole, ClioNode::DbRole::Fallback);
    EXPECT_TRUE(node.etlStarted);
    EXPECT_TRUE(node.cacheIsFull);
    EXPECT_FALSE(node.cacheIsCurrentlyLoading);
}

TEST_F(ClioNodeTest, DeserializationInvalidDbRole)
{
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", 10},
        {"etl_started", false},
        {"cache_is_full", false},
        {"cache_is_currently_loading", false}
    };
    EXPECT_THROW(boost::json::value_to<ClioNode>(jsonValue), std::runtime_error);
}

struct ClioNodeDbRoleTestBundle {
    std::string testName;
    ClioNode::DbRole role;
};

struct ClioNodeDbRoleTest : ClioNodeTest, testing::WithParamInterface<ClioNodeDbRoleTestBundle> {};

INSTANTIATE_TEST_SUITE_P(
    AllDbRoles,
    ClioNodeDbRoleTest,
    testing::Values(
        ClioNodeDbRoleTestBundle{.testName = "ReadOnly", .role = ClioNode::DbRole::ReadOnly},
        ClioNodeDbRoleTestBundle{.testName = "NotWriter", .role = ClioNode::DbRole::NotWriter},
        ClioNodeDbRoleTestBundle{.testName = "Writer", .role = ClioNode::DbRole::Writer},
        ClioNodeDbRoleTestBundle{.testName = "Fallback", .role = ClioNode::DbRole::Fallback},
        ClioNodeDbRoleTestBundle{
            .testName = "FallbackRecovery",
            .role = ClioNode::DbRole::FallbackRecovery
        }
    ),
    tests::util::kNameGenerator
);

TEST_P(ClioNodeDbRoleTest, Serialization)
{
    auto const param = GetParam();
    ClioNode const node{
        .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()()),
        .updateTime = updateTime,
        .dbRole = param.role,
        .etlStarted = false,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = false
    };
    auto const jsonValue = boost::json::value_from(node);
    EXPECT_EQ(jsonValue.as_object().at("db_role").as_int64(), static_cast<int64_t>(param.role));
}

TEST_P(ClioNodeDbRoleTest, Deserialization)
{
    auto const param = GetParam();
    boost::json::value const jsonValue = {
        {"update_time", updateTimeStr},
        {"db_role", static_cast<int64_t>(param.role)},
        {"etl_started", false},
        {"cache_is_full", false},
        {"cache_is_currently_loading", false}
    };
    auto const node = boost::json::value_to<ClioNode>(jsonValue);
    EXPECT_EQ(node.dbRole, param.role);
}

struct ClioNodeFromTestBundle {
    std::string testName;
    bool readOnly;
    bool fallback;
    bool fallbackRecovery;
    bool writing;
    bool etlStarted;
    bool cacheIsFull;
    bool cacheIsCurrentlyLoading;
    ClioNode::DbRole expectedRole;
};

struct ClioNodeFromTest : ClioNodeTest, testing::WithParamInterface<ClioNodeFromTestBundle> {
    std::shared_ptr<boost::uuids::uuid> uuid =
        std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()());

    MockWriterState writerState;
    MockLedgerCacheLoadingState cacheLoadingState;
};

INSTANTIATE_TEST_SUITE_P(
    AllWriterStates,
    ClioNodeFromTest,
    testing::Values(
        ClioNodeFromTestBundle{
            .testName = "ReadOnly",
            .readOnly = true,
            .fallback = false,
            .fallbackRecovery = false,
            .writing = false,
            .etlStarted = false,
            .cacheIsFull = false,
            .cacheIsCurrentlyLoading = false,
            .expectedRole = ClioNode::DbRole::ReadOnly
        },
        ClioNodeFromTestBundle{
            .testName = "Fallback",
            .readOnly = false,
            .fallback = true,
            .fallbackRecovery = false,
            .writing = false,
            .etlStarted = false,
            .cacheIsFull = false,
            .cacheIsCurrentlyLoading = false,
            .expectedRole = ClioNode::DbRole::Fallback
        },
        ClioNodeFromTestBundle{
            .testName = "NotWriter",
            .readOnly = false,
            .fallback = false,
            .fallbackRecovery = false,
            .writing = false,
            .etlStarted = true,
            .cacheIsFull = false,
            .cacheIsCurrentlyLoading = false,
            .expectedRole = ClioNode::DbRole::NotWriter
        },
        ClioNodeFromTestBundle{
            .testName = "Writer",
            .readOnly = false,
            .fallback = false,
            .fallbackRecovery = false,
            .writing = true,
            .etlStarted = true,
            .cacheIsFull = true,
            .cacheIsCurrentlyLoading = true,
            .expectedRole = ClioNode::DbRole::Writer
        },
        ClioNodeFromTestBundle{
            .testName = "FallbackRecovery",
            .readOnly = false,
            .fallback = false,
            .fallbackRecovery = true,
            .writing = false,
            .etlStarted = false,
            .cacheIsFull = false,
            .cacheIsCurrentlyLoading = false,
            .expectedRole = ClioNode::DbRole::FallbackRecovery
        }
    ),
    tests::util::kNameGenerator
);

TEST_P(ClioNodeFromTest, FromWriterState)
{
    auto const& param = GetParam();

    EXPECT_CALL(writerState, isReadOnly()).WillOnce(testing::Return(param.readOnly));
    if (not param.readOnly) {
        EXPECT_CALL(writerState, isFallback()).WillOnce(testing::Return(param.fallback));
        if (not param.fallback) {
            EXPECT_CALL(writerState, isFallbackRecovery())
                .WillOnce(testing::Return(param.fallbackRecovery));
            if (not param.fallbackRecovery) {
                EXPECT_CALL(writerState, isWriting()).WillOnce(testing::Return(param.writing));
            }
        }
    }
    EXPECT_CALL(writerState, isEtlStarted()).WillOnce(testing::Return(param.etlStarted));
    EXPECT_CALL(writerState, isCacheFull()).WillOnce(testing::Return(param.cacheIsFull));
    EXPECT_CALL(cacheLoadingState, isCurrentlyLoading())
        .WillOnce(testing::Return(param.cacheIsCurrentlyLoading));

    auto const beforeTime = std::chrono::system_clock::now();
    auto const node = ClioNode::from(uuid, writerState, cacheLoadingState);
    auto const afterTime = std::chrono::system_clock::now();

    EXPECT_EQ(node.uuid, uuid);
    EXPECT_EQ(node.dbRole, param.expectedRole);
    EXPECT_EQ(node.etlStarted, param.etlStarted);
    EXPECT_EQ(node.cacheIsFull, param.cacheIsFull);
    EXPECT_EQ(node.cacheIsCurrentlyLoading, param.cacheIsCurrentlyLoading);
    EXPECT_GE(node.updateTime, beforeTime);
    EXPECT_LE(node.updateTime, afterTime);
}
