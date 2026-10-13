#include "cluster/Metrics.hpp"

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"

#include <memory>

namespace cluster {

Metrics::Metrics()
{
    nodesInClusterMetric_.set(1);  // The node always sees itself
    // Sets the metric value; the member itself is initialized in the class definition
    isHealthy_ = true;  // NOLINT(cppcoreguidelines-prefer-member-initializer)
}

void
Metrics::onNewState(ClioNode::CUuid, std::shared_ptr<Backend::ClusterData const> clusterData)
{
    if (clusterData->has_value()) {
        isHealthy_ = true;
        nodesInClusterMetric_.set(clusterData->value().size());
    } else {
        isHealthy_ = false;
        nodesInClusterMetric_.set(1);
    }
}

}  // namespace cluster
