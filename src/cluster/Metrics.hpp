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

#pragma once

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <memory>

namespace cluster {

class Metrics {
    util::prometheus::GaugeInt& nodesInClusterMetric_ = PrometheusService::gaugeInt(
        "cluster_nodes_total_number",
        {},
        "Total number of nodes this node can detect in the cluster."
    );
    util::prometheus::Bool isHealthy_ = PrometheusService::boolMetric(
        "cluster_communication_is_healthy",
        {},
        "Whether cluster communication service is operating healthy (1 - healthy, 0 - we have a problem)"
    );

public:
    Metrics()
    {
        nodesInClusterMetric_.set(1);  // The node always sees itself
        isHealthy_ = true;
    }

    void
    onNewState(ClioNode::cUUID, std::shared_ptr<Backend::ClusterData const> clusterData)
    {
        isHealthy_ = clusterData->has_value();
        if (clusterData->has_value()) {
            nodesInClusterMetric_.set(clusterData->value().size());
        }
    }
};

}  // namespace cluster
