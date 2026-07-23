#pragma once

#include "cluster/Backend.hpp"
#include "cluster/CacheLoaderDecider.hpp"
#include "cluster/Concepts.hpp"
#include "cluster/Metrics.hpp"
#include "cluster/WriterDecider.hpp"
#include "data/BackendInterface.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "etl/SystemState.hpp"
#include "etl/WriterState.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>

namespace cluster {

/**
 * @brief Service to post and read messages to/from the cluster. It uses a backend to communicate
 * with the cluster.
 */
class ClusterCommunicationService : public ClusterCommunicationServiceTag {
    // TODO: Use util::async::CoroExecutionContext after https://github.com/XRPLF/clio/issues/1973
    // is implemented
    boost::asio::thread_pool ctx_{1};
    Backend backend_;
    Metrics metrics_;
    WriterDecider writerDecider_;
    CacheLoaderDecider cacheLoaderDecider_;

public:
    static constexpr std::chrono::milliseconds kDefaultReadInterval{1000};
    static constexpr std::chrono::milliseconds kDefaultWriteInterval{1000};

    /**
     * @brief Construct a new Cluster Communication Service object.
     *
     * @param backend The backend to use for communication.
     * @param writerState The state showing whether clio is writing to the database.
     * @param cacheLoadingState State controlling cache loading permission for this node.
     * @param readInterval The interval to read messages from the cluster.
     * @param writeInterval The interval to write messages to the cluster.
     */
    ClusterCommunicationService(
        std::shared_ptr<data::BackendInterface> backend,
        std::unique_ptr<etl::WriterStateInterface> writerState,
        std::unique_ptr<data::LedgerCacheLoadingStateInterface> cacheLoadingState,
        std::chrono::steady_clock::duration readInterval = kDefaultReadInterval,
        std::chrono::steady_clock::duration writeInterval = kDefaultWriteInterval
    );

    ~ClusterCommunicationService() override;

    ClusterCommunicationService(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService(ClusterCommunicationService const&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService const&) = delete;

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

    /**
     * @brief Result of ClusterCommunicationService::make().
     *
     * The @c cacheLoadingState is a clone whose allowLoading() is connected to the state owned by
     * the service, so the caller can pass it to the cache loader.
     */
    struct MakeResult {
        std::unique_ptr<ClusterCommunicationService> service;  ///< The constructed service
        std::unique_ptr<data::LedgerCacheLoadingStateInterface const>
            cacheLoadingState;  ///< Clone of cache loading state for use by the cache loader
    };

    /**
     * @brief Factory method: construct the service and return a cache loading state for the caller.
     *
     * Reads the @c cache.limit_load_in_cluster config flag: if true, loading is immediately
     * allowed (single-node mode); if false, the cluster will gate permission via
     * CacheLoaderDecider.
     *
     * @param config The application configuration
     * @param backend The data backend
     * @param state The shared ETL system state
     * @return A MakeResult containing the service and a cache loading state clone
     */
    static MakeResult
    make(
        util::config::ClioConfigDefinition const& config,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<etl::SystemState> state
    );
};

}  // namespace cluster
