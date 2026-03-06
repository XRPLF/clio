//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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
#include "cluster/CacheLoaderDecider.hpp"
#include "cluster/ClioNode.hpp"
#include "util/MockLedgerCacheLoadingState.hpp"

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

enum class CacheLoaderExpectedAction { AllowLoading, NoAction };

struct CacheLoaderNodeParams {
    uint8_t uuidValue;
    bool cacheIsFull = false;
    bool cacheIsCurrentlyLoading = false;
};

struct CacheLoaderDeciderTestParams {
    std::string testName;
    uint8_t selfUuidValue;
    std::vector<CacheLoaderNodeParams> nodes;
    CacheLoaderExpectedAction expectedAction;
    bool useEmptyClusterData = false;
};

struct CacheLoaderDeciderTest : testing::TestWithParam<CacheLoaderDeciderTestParams> {
    ~CacheLoaderDeciderTest() override
    {
        ctx.stop();
        ctx.join();
    }

    boost::asio::thread_pool ctx{1};
    std::unique_ptr<MockLedgerCacheLoadingState> cacheLoadingState =
        std::make_unique<MockLedgerCacheLoadingState>();
    MockLedgerCacheLoadingState& cacheLoadingStateRef = *cacheLoadingState;

    static ClioNode
    makeNode(boost::uuids::uuid const& uuid, bool cacheIsFull, bool cacheIsCurrentlyLoading)
    {
        return ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(uuid),
            .updateTime = std::chrono::system_clock::now(),
            .dbRole = ClioNode::DbRole::NotWriter,
            .etlStarted = true,
            .cacheIsFull = cacheIsFull,
            .cacheIsCurrentlyLoading = cacheIsCurrentlyLoading,
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

TEST_P(CacheLoaderDeciderTest, CacheLoaderSelection)
{
    auto const& params = GetParam();

    auto const selfUuid = makeUuid(params.selfUuidValue);

    CacheLoaderDecider decider{ctx, std::move(cacheLoadingState)};

    auto clonedState = std::make_unique<MockLedgerCacheLoadingState>();

    switch (params.expectedAction) {
        case CacheLoaderExpectedAction::AllowLoading:
            EXPECT_CALL(*clonedState, allowLoading());
            EXPECT_CALL(cacheLoadingStateRef, clone())
                .WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            break;
        case CacheLoaderExpectedAction::NoAction:
            if (not params.useEmptyClusterData) {
                EXPECT_CALL(cacheLoadingStateRef, clone())
                    .WillOnce(testing::Return(testing::ByMove(std::move(clonedState))));
            }
            break;
    }

    std::shared_ptr<Backend::ClusterData> clusterData;
    ClioNode::CUuid selfIdPtr;

    if (params.useEmptyClusterData) {
        clusterData = std::make_shared<Backend::ClusterData>(
            std::unexpected(std::string("Communication failed"))
        );
        selfIdPtr = std::make_shared<boost::uuids::uuid>(selfUuid);
    } else {
        std::vector<ClioNode> nodes;
        nodes.reserve(params.nodes.size());
        for (auto const& nodeParam : params.nodes) {
            auto node = makeNode(
                makeUuid(nodeParam.uuidValue),
                nodeParam.cacheIsFull,
                nodeParam.cacheIsCurrentlyLoading
            );
            if (nodeParam.uuidValue == params.selfUuidValue) {
                selfIdPtr = node.uuid;
            }
            nodes.push_back(std::move(node));
        }
        clusterData = std::make_shared<Backend::ClusterData>(std::move(nodes));
    }

    decider.onNewState(selfIdPtr, clusterData);

    ctx.join();
}

INSTANTIATE_TEST_SUITE_P(
    CacheLoaderDeciderTests,
    CacheLoaderDeciderTest,
    testing::Values(
        CacheLoaderDeciderTestParams{
            .testName = "SelfCacheIsFullNoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = true},
                 {.uuidValue = 0x02, .cacheIsFull = false}},
            .expectedAction = CacheLoaderExpectedAction::NoAction
        },
        CacheLoaderDeciderTestParams{
            .testName = "SelfIsFirstNotFullByUuid_AllowLoading",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = false},
                 {.uuidValue = 0x02, .cacheIsFull = false}},
            .expectedAction = CacheLoaderExpectedAction::AllowLoading
        },
        CacheLoaderDeciderTestParams{
            .testName = "OtherNodeIsFirstNotFullByUuid_NoAction",
            .selfUuidValue = 0x02,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = false},
                 {.uuidValue = 0x02, .cacheIsFull = false}},
            .expectedAction = CacheLoaderExpectedAction::NoAction
        },
        CacheLoaderDeciderTestParams{
            .testName = "ShuffledNodes_SelfIsFirstNotFull_AllowLoading",
            .selfUuidValue = 0x02,
            .nodes =
                {{.uuidValue = 0x04, .cacheIsFull = false},
                 {.uuidValue = 0x02, .cacheIsFull = false},
                 {.uuidValue = 0x03, .cacheIsFull = true}},
            .expectedAction = CacheLoaderExpectedAction::AllowLoading
        },
        CacheLoaderDeciderTestParams{
            .testName = "FullNodesExcludedFromElection",
            .selfUuidValue = 0x03,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = true},
                 {.uuidValue = 0x02, .cacheIsFull = true},
                 {.uuidValue = 0x03, .cacheIsFull = false}},
            .expectedAction = CacheLoaderExpectedAction::AllowLoading
        },
        CacheLoaderDeciderTestParams{
            .testName = "SomeoneIsCurrentlyLoading_NoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = false, .cacheIsCurrentlyLoading = false},
                 {.uuidValue = 0x02, .cacheIsFull = false, .cacheIsCurrentlyLoading = true}},
            .expectedAction = CacheLoaderExpectedAction::NoAction
        },
        CacheLoaderDeciderTestParams{
            .testName = "SelfIsCurrentlyLoading_NoAction",
            .selfUuidValue = 0x01,
            .nodes =
                {{.uuidValue = 0x01, .cacheIsFull = false, .cacheIsCurrentlyLoading = true},
                 {.uuidValue = 0x02, .cacheIsFull = false, .cacheIsCurrentlyLoading = false}},
            .expectedAction = CacheLoaderExpectedAction::NoAction
        },
        CacheLoaderDeciderTestParams{
            .testName = "SingleNodeCluster_SelfAllowLoading",
            .selfUuidValue = 0x01,
            .nodes = {{.uuidValue = 0x01, .cacheIsFull = false}},
            .expectedAction = CacheLoaderExpectedAction::AllowLoading
        },
        CacheLoaderDeciderTestParams{
            .testName = "EmptyClusterData_NoAction",
            .selfUuidValue = 0x01,
            .nodes = {},
            .expectedAction = CacheLoaderExpectedAction::NoAction,
            .useEmptyClusterData = true
        }
    ),
    [](testing::TestParamInfo<CacheLoaderDeciderTestParams> const& info) {
        return info.param.testName;
    }
);
