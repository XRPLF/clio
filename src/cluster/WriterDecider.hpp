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
#include "cluster/impl/FallbackRecoveryTimer.hpp"
#include "etl/WriterState.hpp"

#include <boost/asio/thread_pool.hpp>

#include <chrono>
#include <memory>

namespace cluster {

/**
 * @brief Decides which node in the cluster should be the writer based on cluster state.
 *
 * This class monitors cluster state changes and determines whether the current node
 * should act as the writer to the database.
 *
 * ## Election (normal operation)
 *
 * All non-ReadOnly nodes are sorted by UUID.  The first node with @c etlStarted and
 * @c cacheIsFull is elected writer.  If no fully-ready node exists, the first node
 * with @c etlStarted is chosen.  All others give up writing.
 *
 * ## Fallback mode
 *
 * Fallback is the slower but more reliable mechanism based on database write-conflict
 * detection (a node waits ~10 s of DB silence before writing).  The cluster enters
 * fallback whenever any non-ReadOnly node publishes @c DbRole::Fallback — for example
 * during a rolling upgrade when an old node without cluster-coordination support is
 * present.
 *
 * ## Fallback recovery
 *
 * To avoid the cluster staying in fallback indefinitely, a recovery timer is started
 * when this node enters fallback.  After the timer fires the node enters
 * @c DbRole::FallbackRecovery and coordinates with peers to return to election mode.
 * If any peer is already in @c FallbackRecovery, the node joins immediately (contagion
 * rule), cancelling its own pending timer.
 *
 * ## State machine for `onNewState`
 *
 * @code
 *
 *                        sees any Fallback node
 *   [election mode]  ──────────────────────────────►  [Fallback]
 *   (NotWriter /                                           │
 *    Writer)                                        recovery timer fires
 *       ▲                                           (1 hour)
 *       │                                           OR sees FallbackRecovery
 *       │                                           node (contagion rule)
 *       │                                                  │
 *       │                                                  ▼
 *       │         no Fallback nodes visible       [FallbackRecovery]
 *       └─────────────────────────────────────────────────
 *
 * @endcode
 *
 * Nodes in FallbackRecovery continue the fallback write-race so there is no write
 * availability gap during the coordination phase.
 */
class WriterDecider {
public:
    static constexpr std::chrono::steady_clock::duration kRECOVERY_TIME = std::chrono::hours{1};

private:
    /** @brief Thread pool for spawning asynchronous tasks */
    boost::asio::thread_pool& ctx_;

    /** @brief Interface for controlling the writer state of this node */
    std::unique_ptr<etl::WriterStateInterface> writerState_;

    /**
     * @brief Timer that fires after a delay to initiate fallback recovery.
     *
     * Started when this node enters @c DbRole::Fallback (either via election-mode
     * transition or via an externally triggered fallback).  Cancelled when the node
     * transitions to @c DbRole::FallbackRecovery (timer fired or contagion rule).
     * Copied into spawned task closures by value — all copies share the same
     * underlying mutex-protected state.
     */
    impl::FallbackRecoveryTimer fallbackRecoveryTimer_;

public:
    /**
     * @brief Constructs a WriterDecider.
     *
     * @param ctx          Thread pool for executing asynchronous operations
     * @param writerState  Writer state interface for controlling write operations
     * @param recoveryTime How long to wait in Fallback before attempting recovery
     *                     (defaults to `kRECOVERY_TIME`; pass a short duration in tests)
     */
    WriterDecider(
        boost::asio::thread_pool& ctx,
        std::unique_ptr<etl::WriterStateInterface> writerState,
        std::chrono::steady_clock::duration recoveryTime = kRECOVERY_TIME
    );

    /**
     * @brief Handles cluster state changes and decides whether this node should be the writer.
     *
     * Spawns an asynchronous task that applies the state machine described in the class
     * documentation.  Decisions are based on the @p clusterData snapshot:
     *
     * - If @p clusterData has no value (communication failure), no action is taken.
     * - If self is @c ReadOnly, writing is given up unconditionally.
     * - If self is @c Fallback and a @c FallbackRecovery node is visible, the contagion
     *   rule applies: this node also enters @c FallbackRecovery and the recovery timer
     *   is cancelled.
     * - If self is @c Fallback and the recovery timer is not running, it is started
     *   (handles the case where fallback was triggered externally, e.g. by Monitor).
     * - If self is @c FallbackRecovery and no @c Fallback nodes are visible, the
     *   recovery coordination is complete: writing is given up and the fallback recovery
     *   flag is cleared so the node enters election mode on the next cycle.
     * - If self is in election mode and any @c Fallback node is visible, this node
     *   switches to @c Fallback and the recovery timer is started.
     * - Otherwise, election proceeds: nodes are sorted by UUID and the first fully-ready
     *   (@c etlStarted && @c cacheIsFull) non-ReadOnly node is elected writer.
     *
     * @param selfId The UUID of the current node
     * @param clusterData Shared pointer to current cluster data; may be empty if
     *   communication failed
     */
    void
    onNewState(ClioNode::CUuid selfId, std::shared_ptr<Backend::ClusterData const> clusterData);
};

}  // namespace cluster
