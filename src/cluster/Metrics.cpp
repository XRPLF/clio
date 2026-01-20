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

#include "cluster/Metrics.hpp"

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"

#include <memory>

namespace cluster {

Metrics::Metrics()
{
    nodesInClusterMetric_.set(1);  // The node always sees itself
    isHealthy_ = true;
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
