#pragma once

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "data/LedgerCacheLoadingState.hpp"

#include <boost/asio/thread_pool.hpp>

#include <memory>

namespace cluster {

/**
 * @brief Decides which node in the cluster should load the ledger cache.
 *
 * This class monitors cluster state changes and determines whether the current node
 * should begin loading the ledger cache from the backend. The decision is made by:
 * 1. Doing nothing if this node's cache is already full
 * 2. Doing nothing if any node in the cluster is currently loading the cache
 * 3. Sorting all nodes whose cache is not yet full by UUID for deterministic ordering
 * 4. Permitting loading on this node if it is the first in the sorted list
 *
 * This ensures at most one node in the cluster loads the cache at a time.
 */
class CacheLoaderDecider {
    /** @brief Thread pool for spawning asynchronous tasks */
    boost::asio::thread_pool& ctx_;

    /** @brief Interface for controlling cache loading permission of this node */
    std::unique_ptr<data::LedgerCacheLoadingStateInterface> cacheLoadingState_;

public:
    /**
     * @brief Constructs a CacheLoaderDecider.
     *
     * @param ctx Thread pool for executing asynchronous operations
     * @param cacheLoadingState Cache loading state interface for permitting cache load
     */
    CacheLoaderDecider(
        boost::asio::thread_pool& ctx,
        std::unique_ptr<data::LedgerCacheLoadingStateInterface> cacheLoadingState
    );

    /**
     * @brief Handles cluster state changes and decides whether this node should load the cache.
     *
     * This method is called when cluster state changes. It asynchronously:
     * - Does nothing if this node's cache is already full
     * - Does nothing if any node in the cluster is currently loading the cache
     * - Sorts all not-yet-full nodes by UUID to establish a deterministic order
     * - Permits cache loading on this node if it is first in the sorted list
     *
     * @param selfId The UUID of the current node
     * @param clusterData Shared pointer to current cluster data; may be empty if communication
     * failed
     */
    void
    onNewState(ClioNode::CUuid selfId, std::shared_ptr<Backend::ClusterData const> clusterData);
};

}  // namespace cluster
