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

#include "cluster/ClioNode.hpp"
#include "cluster/ClusterCommunicationServiceInterface.hpp"
#include "data/BackendInterface.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace cluster {

/**
 * @brief Service to post and read messages to/from the cluster. It uses a backend to communicate with the cluster.
 */
class ClusterCommunicationService : public ClusterCommunicationServiceInterface {
    util::prometheus::GaugeInt& nodesInClusterMetric_ = PrometheusService::gaugeInt(
        "cluster_nodes_total_number",
        {},
        "Total number of nodes this node can detect in the cluster."
    );
    util::prometheus::Bool isHealthy_ = PrometheusService::boolMetric(
        "cluster_communication_is_healthy",
        {},
        "Whether cluster communicaton service is operating healthy (1 - healthy, 0 - we have a problem)"
    );

    // TODO: Use util::async::CoroExecutionContext after https://github.com/XRPLF/clio/issues/1973 is implemented
    boost::asio::thread_pool ctx_{1};
    boost::asio::strand<boost::asio::thread_pool::executor_type> strand_ = boost::asio::make_strand(ctx_);

    util::Logger log_{"ClusterCommunication"};

    std::shared_ptr<data::BackendInterface> backend_;

    std::chrono::steady_clock::duration readInterval_;
    std::chrono::steady_clock::duration writeInterval_;

    ClioNode selfData_;
    std::vector<ClioNode> otherNodesData_;

    bool stopped_ = false;

public:
    static constexpr std::chrono::milliseconds kDEFAULT_READ_INTERVAL{2100};
    static constexpr std::chrono::milliseconds kDEFAULT_WRITE_INTERVAL{1200};
    /**
     * @brief Construct a new Cluster Communication Service object.
     *
     * @param backend The backend to use for communication.
     * @param readInterval The interval to read messages from the cluster.
     * @param writeInterval The interval to write messages to the cluster.
     */
    ClusterCommunicationService(
        std::shared_ptr<data::BackendInterface> backend,
        std::chrono::steady_clock::duration readInterval = kDEFAULT_READ_INTERVAL,
        std::chrono::steady_clock::duration writeInterval = kDEFAULT_WRITE_INTERVAL
    );

    ~ClusterCommunicationService() override;

    /**
     * @brief Start the service.
     */
    void
    run();

    /**
     * @brief Stop the service.
     */
    void
    stop();

    ClusterCommunicationService(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService(ClusterCommunicationService const&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService const&) = delete;

    /**
     * @brief Get the UUID of the current node.
     *
     * @return The UUID of the current node.
     */
    std::shared_ptr<boost::uuids::uuid>
    selfUuid() const;

    /**
     * @brief Get the data of the current node.
     *
     * @return The data of the current node.
     */
    ClioNode
    selfData() const override;

    /**
     * @brief Get the data of all nodes in the cluster (including self).
     *
     * @return The data of all nodes in the cluster.
     */
    std::vector<ClioNode>
    clusterData() const override;

private:
    void
    doRead(boost::asio::yield_context yield);

    void
    doWrite();
};

}  // namespace cluster
