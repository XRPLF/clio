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
#include "cluster/ClusterCommunicationService.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TimeUtils.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace cluster;

struct ClusterCommunicationServiceTest : util::prometheus::WithPrometheus, MockBackendTestStrict {
    ClusterCommunicationService clusterCommunicationService{
        backend_,
        std::chrono::milliseconds{5},
        std::chrono::milliseconds{9}
    };

    util::prometheus::GaugeInt& nodesInClusterMetric = PrometheusService::gaugeInt("cluster_nodes_total_number", {});
    util::prometheus::Bool isHealthyMetric = PrometheusService::boolMetric("cluster_communication_is_healthy", {});

    std::mutex mtx;
    std::condition_variable cv;

    void
    notify()
    {
        std::unique_lock const lock{mtx};
        cv.notify_one();
    }

    void
    wait()
    {
        std::unique_lock lock{mtx};
        cv.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::milliseconds{100});
    }
};

TEST_F(ClusterCommunicationServiceTest, Write)
{
    auto const selfUuid = *clusterCommunicationService.selfUuid();

    auto const nowStr = util::systemTpToUtcStr(std::chrono::system_clock::now(), ClioNode::kTIME_FORMAT);
    auto const nowStrPrefix = nowStr.substr(0, nowStr.size() - 3);

    EXPECT_CALL(*backend_, writeNodeMessage(selfUuid, testing::_)).WillOnce([&](auto&&, std::string const& jsonStr) {
        auto const jv = boost::json::parse(jsonStr);
        ASSERT_TRUE(jv.is_object());
        auto const& obj = jv.as_object();
        ASSERT_TRUE(obj.contains("update_time"));
        ASSERT_TRUE(obj.at("update_time").is_string());
        EXPECT_THAT(std::string{obj.at("update_time").as_string()}, testing::StartsWith(nowStrPrefix));

        notify();
    });

    clusterCommunicationService.run();
    wait();
}

TEST_F(ClusterCommunicationServiceTest, Read_FetchFailed)
{
    EXPECT_TRUE(isHealthyMetric);
    EXPECT_CALL(*backend_, writeNodeMessage).Times(2).WillOnce([](auto&&, auto&&) {}).WillOnce([this](auto&&, auto&&) {
        notify();
    });
    EXPECT_CALL(*backend_, fetchClioNodesData).WillOnce([](auto&&) { return std::unexpected{"Failed"}; });

    clusterCommunicationService.run();
    wait();
    EXPECT_FALSE(isHealthyMetric);
}

TEST_F(ClusterCommunicationServiceTest, Read_GotInvalidJson)
{
    EXPECT_TRUE(isHealthyMetric);
    EXPECT_CALL(*backend_, writeNodeMessage).Times(2).WillOnce([](auto&&, auto&&) {}).WillOnce([this](auto&&, auto&&) {
        notify();
    });
    EXPECT_CALL(*backend_, fetchClioNodesData).WillOnce([](auto&&) {
        return std::vector<std::pair<boost::uuids::uuid, std::string>>{
            {boost::uuids::random_generator()(), "invalid json"}
        };
    });

    clusterCommunicationService.run();
    wait();
    EXPECT_FALSE(isHealthyMetric);
}

TEST_F(ClusterCommunicationServiceTest, Read_GotInvalidNodeData)
{
    EXPECT_TRUE(isHealthyMetric);
    EXPECT_CALL(*backend_, writeNodeMessage).Times(2).WillOnce([](auto&&, auto&&) {}).WillOnce([this](auto&&, auto&&) {
        notify();
    });
    EXPECT_CALL(*backend_, fetchClioNodesData).WillOnce([](auto&&) {
        return std::vector<std::pair<boost::uuids::uuid, std::string>>{{boost::uuids::random_generator()(), "{}"}};
    });

    clusterCommunicationService.run();
    wait();
    EXPECT_FALSE(isHealthyMetric);
}

TEST_F(ClusterCommunicationServiceTest, Read_Success)
{
    EXPECT_TRUE(isHealthyMetric);
    EXPECT_EQ(nodesInClusterMetric.value(), 1);
    std::vector<ClioNode> otherNodesData = {
        ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()()),
            .updateTime = util::systemTpFromUtcStr("2015-05-15T12:00:00Z", ClioNode::kTIME_FORMAT).value()
        },
        ClioNode{
            .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()()),
            .updateTime = util::systemTpFromUtcStr("2015-05-15T12:00:01Z", ClioNode::kTIME_FORMAT).value()
        },
    };
    auto const selfUuid = *clusterCommunicationService.selfUuid();

    EXPECT_CALL(*backend_, writeNodeMessage).Times(2).WillOnce([](auto&&, auto&&) {}).WillOnce([&](auto&&, auto&&) {
        auto const clusterData = clusterCommunicationService.clusterData();
        ASSERT_EQ(clusterData.size(), otherNodesData.size() + 1);
        for (auto const& node : otherNodesData) {
            auto const it =
                std::ranges::find_if(clusterData, [&](ClioNode const& n) { return *(n.uuid) == *(node.uuid); });
            EXPECT_NE(it, clusterData.cend()) << boost::uuids::to_string(*node.uuid);
        }
        auto const selfUuid = clusterCommunicationService.selfUuid();
        auto const it =
            std::ranges::find_if(clusterData, [&selfUuid](ClioNode const& node) { return node.uuid == selfUuid; });
        EXPECT_NE(it, clusterData.end());

        notify();
    });

    EXPECT_CALL(*backend_, fetchClioNodesData).WillOnce([&](auto&&) {
        std::vector<std::pair<boost::uuids::uuid, std::string>> result = {
            {selfUuid, R"json({"update_time": "2015-05-15:12:00:00"})json"},
        };

        for (auto const& node : otherNodesData) {
            boost::json::value jsonValue;
            boost::json::value_from(node, jsonValue);
            result.emplace_back(*node.uuid, boost::json::serialize(jsonValue));
        }
        return result;
    });

    clusterCommunicationService.run();
    wait();
    EXPECT_TRUE(isHealthyMetric);
    EXPECT_EQ(nodesInClusterMetric.value(), 3);
}
