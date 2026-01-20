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

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "cluster/WriterDecider.hpp"
#include "util/MockWriterState.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace cluster;

enum class ExpectedAction { StartWriting, GiveUpWriting, NoAction, SetFallback };

struct WriterDeciderTestParams {
    std::string testName;
    uint8_t selfUuidValue;
    std::vector<std::pair<uint8_t, ClioNode::DbRole>> nodes;
    ExpectedAction expectedAction;
    bool useEmptyClusterData = false;
};

struct WriterDeciderTest : testing::TestWithParam<WriterDeciderTestParams> {
    ~WriterDeciderTest() override
    {
        ctx.stop();
        ctx.join();
    }

    boost::asio::thread_pool ctx{1};
    std::unique_ptr<MockWriterState> writerState = std::make_unique<MockWriterState>();
    MockWriterState& writerStateRef = *writerState;

    static ClioNode
    makeNode(boost::uuids::uuid const& uuid, ClioNode::DbRole role)
    {
        return ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(uuid),
            .updateTime = std::chrono::system_clock::now(),
            .dbRole = role
        };
    }

    static boost::uuids::uuid
    makeUuid(uint8_t value)
    {
        boost::uuids::uuid uuid{};
        std::ranges::fill(uuid, value);
        return uuid;
    }
};

TEST_P(WriterDeciderTest, WriterSelection)
{
    auto const& params = GetParam();

    auto const selfUuid = makeUuid(params.selfUuidValue);

    WriterDecider decider{ctx, std::move(writerState)};

    auto clonedState = std::make_unique<MockWriterState>();

    // Set up expectations based on expected action
    switch (params.expectedAction) {
        case ExpectedAction::StartWriting:
            EXPECT_CALL(*clonedState, startWriting());
            EXPECT_CALL(writerStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            break;
        case ExpectedAction::GiveUpWriting:
            EXPECT_CALL(*clonedState, giveUpWriting());
            EXPECT_CALL(writerStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            break;
        case ExpectedAction::SetFallback:
            EXPECT_CALL(*clonedState, setWriterDecidingFallback());
            EXPECT_CALL(writerStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            break;
        case ExpectedAction::NoAction:
            if (not params.useEmptyClusterData) {
                // For all-ReadOnly case, we still clone but don't call any action
                EXPECT_CALL(writerStateRef, clone()).WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            }
            // For empty cluster data, clone is never called
            break;
    }

    std::shared_ptr<Backend::ClusterData> clusterData;
    ClioNode::CUuid selfIdPtr;

    if (params.useEmptyClusterData) {
        clusterData = std::make_shared<Backend::ClusterData>(std::unexpected(std::string("Communication failed")));
        selfIdPtr = std::make_shared<boost::uuids::uuid>(selfUuid);
    } else {
        std::vector<ClioNode> nodes;
        nodes.reserve(params.nodes.size());
        for (auto const& [uuidValue, role] : params.nodes) {
            auto node = makeNode(makeUuid(uuidValue), role);
            if (uuidValue == params.selfUuidValue) {
                selfIdPtr = node.uuid;  // Use the same shared_ptr as in the node
            }
            nodes.push_back(std::move(node));
        }
        clusterData = std::make_shared<Backend::ClusterData>(std::move(nodes));
    }

    decider.onNewState(selfIdPtr, clusterData);

    ctx.join();
}

INSTANTIATE_TEST_SUITE_P(
    WriterDeciderTests,
    WriterDeciderTest,
    testing::Values(
        WriterDeciderTestParams{
            .testName = "SelfNodeIsSelectedAsWriter",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::Writer}, {0x02, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "OtherNodeIsSelectedAsWriter",
            .selfUuidValue = 0x02,
            .nodes = {{0x01, ClioNode::DbRole::Writer}, {0x02, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "NodesAreSortedByUUID",
            .selfUuidValue = 0x02,
            .nodes =
                {{0x03, ClioNode::DbRole::Writer}, {0x02, ClioNode::DbRole::Writer}, {0x01, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "FirstNodeAfterReadOnlyIsNotSelf",
            .selfUuidValue = 0x03,
            .nodes =
                {{0x01, ClioNode::DbRole::ReadOnly},
                 {0x02, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "FirstNodeAfterReadOnlyIsSelf",
            .selfUuidValue = 0x02,
            .nodes =
                {{0x01, ClioNode::DbRole::ReadOnly},
                 {0x02, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "AllNodesReadOnlyGiveUpWriting",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::ReadOnly}, {0x02, ClioNode::DbRole::ReadOnly}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "EmptyClusterDataNoActionTaken",
            .selfUuidValue = 0x01,
            .nodes = {},
            .expectedAction = ExpectedAction::NoAction,
            .useEmptyClusterData = true
        },
        WriterDeciderTestParams{
            .testName = "SingleNodeClusterSelfIsWriter",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "NotWriterRoleIsSelectedWhenNoWriterRole",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::NotWriter}, {0x02, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "MixedRolesFirstNonReadOnlyIsSelected",
            .selfUuidValue = 0x03,
            .nodes =
                {{0x01, ClioNode::DbRole::ReadOnly},
                 {0x02, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::NotWriter},
                 {0x04, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "ShuffledNodesAreSortedCorrectly",
            .selfUuidValue = 0x04,
            .nodes =
                {{0x04, ClioNode::DbRole::Writer},
                 {0x01, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::Writer},
                 {0x02, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "ShuffledNodesWithReadOnlySelfIsSelected",
            .selfUuidValue = 0x03,
            .nodes =
                {{0x05, ClioNode::DbRole::Writer},
                 {0x01, ClioNode::DbRole::ReadOnly},
                 {0x04, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::Writer},
                 {0x02, ClioNode::DbRole::ReadOnly}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "SelfIsFallbackNoActionTaken",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::Fallback}, {0x02, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::NoAction
        },
        WriterDeciderTestParams{
            .testName = "OtherNodeIsFallbackSetsFallbackMode",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::Writer}, {0x02, ClioNode::DbRole::Fallback}},
            .expectedAction = ExpectedAction::SetFallback
        },
        WriterDeciderTestParams{
            .testName = "SelfIsReadOnlyOthersAreFallbackGiveUpWriting",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::ReadOnly}, {0x02, ClioNode::DbRole::Fallback}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "MultipleFallbackNodesSelfNotFallbackSetsFallback",
            .selfUuidValue = 0x03,
            .nodes =
                {{0x01, ClioNode::DbRole::Fallback},
                 {0x02, ClioNode::DbRole::Fallback},
                 {0x03, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::SetFallback
        },
        WriterDeciderTestParams{
            .testName = "MixedRolesWithOneFallbackSetsFallback",
            .selfUuidValue = 0x02,
            .nodes =
                {{0x01, ClioNode::DbRole::Writer},
                 {0x02, ClioNode::DbRole::NotWriter},
                 {0x03, ClioNode::DbRole::Fallback},
                 {0x04, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::SetFallback
        },
        WriterDeciderTestParams{
            .testName = "SelfIsLoadingCacheOtherIsWriter",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::LoadingCache}, {0x02, ClioNode::DbRole::Writer}},
            .expectedAction = ExpectedAction::GiveUpWriting
        },
        WriterDeciderTestParams{
            .testName = "OtherNodeIsLoadingCacheSkipToNextWriter",
            .selfUuidValue = 0x02,
            .nodes =
                {{0x01, ClioNode::DbRole::LoadingCache},
                 {0x02, ClioNode::DbRole::Writer},
                 {0x03, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "AllNodesLoadingCacheNoActionTaken",
            .selfUuidValue = 0x01,
            .nodes = {{0x01, ClioNode::DbRole::LoadingCache}, {0x02, ClioNode::DbRole::LoadingCache}},
            .expectedAction = ExpectedAction::NoAction
        },
        WriterDeciderTestParams{
            .testName = "MixedWithLoadingCacheReadOnlyFirstNonReadOnlyNonLoadingCacheSelected",
            .selfUuidValue = 0x03,
            .nodes =
                {{0x01, ClioNode::DbRole::ReadOnly},
                 {0x02, ClioNode::DbRole::LoadingCache},
                 {0x03, ClioNode::DbRole::Writer},
                 {0x04, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::StartWriting
        },
        WriterDeciderTestParams{
            .testName = "LoadingCacheBeforeWriterSkipsLoadingCache",
            .selfUuidValue = 0x04,
            .nodes =
                {{0x01, ClioNode::DbRole::LoadingCache},
                 {0x02, ClioNode::DbRole::LoadingCache},
                 {0x03, ClioNode::DbRole::Writer},
                 {0x04, ClioNode::DbRole::NotWriter}},
            .expectedAction = ExpectedAction::GiveUpWriting
        }
    ),
    [](testing::TestParamInfo<WriterDeciderTestParams> const& info) { return info.param.testName; }
);
