#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "cluster/Metrics.hpp"
#include "util/MockPrometheus.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <expected>
#include <memory>
#include <string>
#include <vector>

using namespace cluster;
using namespace util::prometheus;
using namespace testing;

struct MetricsTest : WithMockPrometheus {
    std::shared_ptr<boost::uuids::uuid> uuid1 =
        std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()());
    std::shared_ptr<boost::uuids::uuid> uuid2 =
        std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()());
    std::shared_ptr<boost::uuids::uuid> uuid3 =
        std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator()());
};

TEST_F(MetricsTest, InitializesMetricsOnConstruction)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics const metrics;
}

TEST_F(MetricsTest, OnNewStateWithValidClusterData)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics metrics;

    ClioNode const node1{
        .uuid = uuid1,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::Writer,
        .etlStarted = true,
        .cacheIsFull = true,
        .cacheIsCurrentlyLoading = false
    };
    ClioNode const node2{
        .uuid = uuid2,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::ReadOnly,
        .etlStarted = true,
        .cacheIsFull = true,
        .cacheIsCurrentlyLoading = false
    };
    ClioNode const node3{
        .uuid = uuid3,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::NotWriter,
        .etlStarted = true,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = false
    };

    std::vector<ClioNode> const nodes = {node1, node2, node3};
    Backend::ClusterData const clusterData =
        std::expected<std::vector<ClioNode>, std::string>(nodes);
    auto sharedClusterData = std::make_shared<Backend::ClusterData>(clusterData);

    EXPECT_CALL(isHealthyMock, set(1));
    EXPECT_CALL(nodesInClusterMock, set(3));

    metrics.onNewState(uuid1, sharedClusterData);
}

TEST_F(MetricsTest, OnNewStateWithEmptyClusterData)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics metrics;

    std::vector<ClioNode> const nodes = {};
    Backend::ClusterData const clusterData =
        std::expected<std::vector<ClioNode>, std::string>(nodes);
    auto sharedClusterData = std::make_shared<Backend::ClusterData>(clusterData);

    EXPECT_CALL(isHealthyMock, set(1));
    EXPECT_CALL(nodesInClusterMock, set(0));

    metrics.onNewState(uuid1, sharedClusterData);
}

TEST_F(MetricsTest, OnNewStateWithFailedClusterData)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics metrics;

    Backend::ClusterData const clusterData =
        std::expected<std::vector<ClioNode>, std::string>(std::unexpected("Connection failed"));
    auto sharedClusterData = std::make_shared<Backend::ClusterData>(clusterData);

    EXPECT_CALL(isHealthyMock, set(0));
    EXPECT_CALL(nodesInClusterMock, set(1));

    metrics.onNewState(uuid1, sharedClusterData);
}

TEST_F(MetricsTest, OnNewStateWithSingleNode)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics metrics;

    ClioNode const node1{
        .uuid = uuid1,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::Writer,
        .etlStarted = true,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = false
    };

    std::vector<ClioNode> const nodes = {node1};
    Backend::ClusterData const clusterData =
        std::expected<std::vector<ClioNode>, std::string>(nodes);
    auto sharedClusterData = std::make_shared<Backend::ClusterData>(clusterData);

    EXPECT_CALL(isHealthyMock, set(1));
    EXPECT_CALL(nodesInClusterMock, set(1));

    metrics.onNewState(uuid1, sharedClusterData);
}

TEST_F(MetricsTest, OnNewStateRecoveryFromFailure)
{
    auto& nodesInClusterMock = makeMock<GaugeInt>("cluster_nodes_total_number", "");
    auto& isHealthyMock = makeMock<GaugeInt>("cluster_communication_is_healthy", "");

    EXPECT_CALL(nodesInClusterMock, set(1));
    EXPECT_CALL(isHealthyMock, set(1));

    Metrics metrics;

    Backend::ClusterData const clusterData1 =
        std::expected<std::vector<ClioNode>, std::string>(std::unexpected("Connection timeout"));
    auto sharedClusterData1 = std::make_shared<Backend::ClusterData>(clusterData1);

    EXPECT_CALL(isHealthyMock, set(0));
    EXPECT_CALL(nodesInClusterMock, set(1));

    metrics.onNewState(uuid1, sharedClusterData1);

    ClioNode const node1{
        .uuid = uuid1,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::Writer,
        .etlStarted = true,
        .cacheIsFull = true,
        .cacheIsCurrentlyLoading = false
    };
    ClioNode const node2{
        .uuid = uuid2,
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = ClioNode::DbRole::ReadOnly,
        .etlStarted = true,
        .cacheIsFull = false,
        .cacheIsCurrentlyLoading = false
    };

    std::vector<ClioNode> const nodes = {node1, node2};
    Backend::ClusterData const clusterData2 =
        std::expected<std::vector<ClioNode>, std::string>(nodes);
    auto sharedClusterData2 = std::make_shared<Backend::ClusterData>(clusterData2);

    EXPECT_CALL(isHealthyMock, set(1));
    EXPECT_CALL(nodesInClusterMock, set(2));

    metrics.onNewState(uuid2, sharedClusterData2);
}
