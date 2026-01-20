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
#include "cluster/impl/RepeatedTask.hpp"
#include "data/BackendInterface.hpp"
#include "etl/WriterState.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <vector>

namespace cluster {

/**
 * @brief Backend communication handler for cluster state synchronization.
 *
 * This class manages reading and writing cluster state information to/from the backend database.
 * It periodically reads the state of other nodes in the cluster and writes the current node's state,
 * enabling cluster-wide coordination and awareness.
 */
class Backend {
public:
    /** @brief Type representing cluster data result - either a vector of nodes or an error message */
    using ClusterData = std::expected<std::vector<ClioNode>, std::string>;

private:
    util::Logger log_{"ClusterCommunication"};

    std::shared_ptr<data::BackendInterface> backend_;
    std::unique_ptr<etl::WriterStateInterface const> writerState_;

    impl::RepeatedTask<boost::asio::thread_pool> readerTask_;
    impl::RepeatedTask<boost::asio::thread_pool> writerTask_;

    ClioNode::Uuid selfUuid_;

    boost::signals2::signal<void(ClioNode::CUuid, std::shared_ptr<ClusterData const>)> onNewState_;

public:
    /**
     * @brief Construct a Backend communication handler.
     *
     * @param ctx The execution context for asynchronous operations
     * @param backend Interface to the backend database
     * @param writerState State indicating whether this node is writing to the database
     * @param readInterval How often to read cluster state from the backend
     * @param writeInterval How often to write this node's state to the backend
     */
    Backend(
        boost::asio::thread_pool& ctx,
        std::shared_ptr<data::BackendInterface> backend,
        std::unique_ptr<etl::WriterStateInterface const> writerState,
        std::chrono::steady_clock::duration readInterval,
        std::chrono::steady_clock::duration writeInterval
    );

    ~Backend();

    Backend(Backend&&) = delete;
    Backend&
    operator=(Backend&&) = delete;
    Backend(Backend const&) = delete;
    Backend&
    operator=(Backend const&) = delete;

    /**
     * @brief Start the backend read and write tasks.
     *
     * Begins periodic reading of cluster state from the backend and writing of this node's state.
     */
    void
    run();

    /**
     * @brief Stop the backend read and write tasks.
     *
     * Stops all periodic tasks and waits for them to complete.
     */
    void
    stop();

    /**
     * @brief Subscribe to new cluster state notifications.
     *
     * @tparam S Callable type accepting (ClioNode::cUUID, ClusterData)
     * @param s Subscriber callback to be invoked when new cluster state is available
     * @return A connection object that can be used to unsubscribe
     */
    template <typename S>
        requires std::invocable<S, ClioNode::CUuid, std::shared_ptr<ClusterData const>>
    boost::signals2::connection
    subscribeToNewState(S&& s)
    {
        return onNewState_.connect(s);
    }

    /**
     * @brief Get the UUID of this node in the cluster.
     *
     * @return The UUID of this node.
     */
    ClioNode::CUuid
    selfId() const;

private:
    ClusterData
    doRead(boost::asio::yield_context yield);

    void
    doWrite();
};

}  // namespace cluster
