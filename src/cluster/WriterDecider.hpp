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
#include "etl/WriterState.hpp"

#include <boost/asio/thread_pool.hpp>

#include <memory>

namespace cluster {

/**
 * @brief Decides which node in the cluster should be the writer based on cluster state.
 *
 * This class monitors cluster state changes and determines whether the current node
 * should act as the writer to the database. The decision is made by:
 * 1. Sorting all nodes by UUID for deterministic ordering
 * 2. Selecting the first node that is allowed to write (not ReadOnly)
 * 3. Activating writing on this node if it's the current node, otherwise deactivating
 *
 * This ensures only one node in the cluster actively writes to the database at a time.
 */
class WriterDecider {
    /** @brief Thread pool for spawning asynchronous tasks */
    boost::asio::thread_pool& ctx_;

    /** @brief Interface for controlling the writer state of this node */
    std::unique_ptr<etl::WriterStateInterface> writerState_;

public:
    /**
     * @brief Constructs a WriterDecider.
     *
     * @param ctx Thread pool for executing asynchronous operations
     * @param writerState Writer state interface for controlling write operations
     */
    WriterDecider(
        boost::asio::thread_pool& ctx,
        std::unique_ptr<etl::WriterStateInterface> writerState
    );

    /**
     * @brief Handles cluster state changes and decides whether this node should be the writer.
     *
     * This method is called when cluster state changes. It asynchronously:
     * - Sorts all nodes by UUID to establish a deterministic order
     * - Identifies the first node allowed to write (not ReadOnly)
     * - Activates writing if this node is selected, otherwise deactivates writing
     * - Logs a warning if no nodes in the cluster are allowed to write
     *
     * @param selfId The UUID of the current node
     * @param clusterData Shared pointer to current cluster data; may be empty if communication
     * failed
     */
    void
    onNewState(ClioNode::CUuid selfId, std::shared_ptr<Backend::ClusterData const> clusterData);
};

}  // namespace cluster
