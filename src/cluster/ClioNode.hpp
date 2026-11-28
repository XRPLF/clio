#pragma once

#include "data/LedgerCacheLoadingState.hpp"
#include "etl/WriterState.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>

namespace cluster {

/**
 * @brief Represents a node in the cluster.
 */
struct ClioNode {
    /**
     * @brief The format of the time to store in the database.
     */
    static constexpr char const* kTimeFormat = "%Y-%m-%dT%H:%M:%SZ";

    /**
     * @brief Database role of a node in the cluster.
     *
     * Roles are used to coordinate which node writes to the database:
     * - ReadOnly: Node is configured to never write (strict read-only mode).
     * - NotWriter: Node can write but is currently not the designated writer.
     * - Writer: Node is actively writing to the database.
     * - Fallback: Node is using the fallback writer decision mechanism (slower but
     *   reliable database-based write-conflict detection).  When any non-ReadOnly node
     *   in the cluster is in this role, the entire cluster switches to fallback mode.
     * - FallbackRecovery: Node has been in Fallback long enough to attempt returning to
     *   election-based writer selection.  The node continues participating in the
     *   fallback write-race while coordinating with peers.  Once all non-ReadOnly nodes
     *   reach this role (or have already returned to election mode), the cluster exits
     *   fallback and performs a normal election.
     */
    enum class DbRole {
        ReadOnly = 0,          ///< Node never writes to the database.
        NotWriter = 1,         ///< Node can write but is not the designated writer.
        Writer = 2,            ///< Node is actively writing to the database.
        Fallback = 3,          ///< Node uses the database-based fallback writer decision.
        FallbackRecovery = 4,  ///< Node is coordinating a return from fallback to election mode.
        Max = 4,               ///< Highest valid role value; used for validation.
    };

    using Uuid = std::shared_ptr<boost::uuids::uuid>;         ///< Shared node UUID.
    using CUuid = std::shared_ptr<boost::uuids::uuid const>;  ///< Shared read-only node UUID.

    Uuid uuid;  ///< The UUID of the node.
    std::chrono::system_clock::time_point
        updateTime;                ///< The time the data about the node was last updated.
    DbRole dbRole;                 ///< The database role of the node
    bool etlStarted;               ///< Whether the ETL monitor has started on this node
    bool cacheIsFull;              ///< Whether the ledger cache is fully loaded on this node
    bool cacheIsCurrentlyLoading;  ///< Whether this node is currently loading the ledger cache

    /**
     * @brief Create a ClioNode from writer state and cache loading state.
     *
     * @param uuid The UUID of the node
     * @param writerState The writer state to determine the node's database role
     * @param cacheLoadingState The cache loading state to determine if cache is being loaded
     * @return A ClioNode with the current time and role derived from writerState
     */
    static ClioNode
    from(
        Uuid uuid,
        etl::WriterStateInterface const& writerState,
        data::LedgerCacheLoadingStateInterface const& cacheLoadingState
    );
};

/**
 * @brief Convert a ClioNode to a JSON object.
 *
 * The UUID is not serialized; it is stored separately in the database.
 *
 * @param jv The JSON value to populate
 * @param node The node to convert
 */
void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node);

/**
 * @brief Convert a JSON object to a ClioNode.
 *
 * Missing optional fields get backward compatible defaults. The UUID of the result is a fresh empty
 * UUID that the caller is expected to fill.
 *
 * @param jv The JSON value to convert
 * @return The converted ClioNode
 * @throws std::runtime_error if the update time cannot be parsed, db_role exceeds DbRole::Max,
 * or a field is missing or mistyped
 */
ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv);

}  // namespace cluster
