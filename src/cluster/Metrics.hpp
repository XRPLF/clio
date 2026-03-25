#pragma once

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <memory>

namespace cluster {

/**
 * @brief Manages Prometheus metrics for cluster communication and node tracking.
 *
 * This class tracks cluster-related metrics including:
 * - Total number of nodes detected in the cluster
 * - Health status of cluster communication
 */
class Metrics {
    /** @brief Gauge tracking the total number of nodes visible in the cluster */
    util::prometheus::GaugeInt& nodesInClusterMetric_ = PrometheusService::gaugeInt(
        "cluster_nodes_total_number",
        {},
        "Total number of nodes this node can detect in the cluster."
    );

    /** @brief Boolean metric indicating whether cluster communication is healthy */
    util::prometheus::Bool isHealthy_ = PrometheusService::boolMetric(
        "cluster_communication_is_healthy",
        {},
        "Whether cluster communication service is operating healthy (1 - healthy, 0 - we have a "
        "problem)"
    );

public:
    /**
     * @brief Constructs a Metrics instance and initializes metrics.
     *
     * Sets the initial node count to 1 (self) and marks communication as healthy.
     */
    Metrics();

    /**
     * @brief Updates metrics based on new cluster state.
     *
     * This callback is invoked when cluster state changes. It updates:
     * - Health status based on whether cluster data is available
     * - Node count to reflect the current cluster size
     *
     * @param uuid The UUID of the node (unused in current implementation)
     * @param clusterData Shared pointer to the current cluster data; may be empty if communication
     * failed
     */
    void
    onNewState(ClioNode::CUuid uuid, std::shared_ptr<Backend::ClusterData const> clusterData);
};

}  // namespace cluster
