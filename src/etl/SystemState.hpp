//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <memory>

namespace etl {

/**
 * @brief Represents the state of the ETL subsystem.
 */
struct SystemState {
    /**
     * @brief Factory method to create a SystemState instance.
     *
     * @param config The configuration to use for initializing the system state
     * @return A shared pointer to the newly created SystemState
     */
    static std::shared_ptr<SystemState>
    makeSystemState(util::config::ClioConfigDefinition const& config)
    {
        auto state = std::make_shared<SystemState>();
        state->isStrictReadonly = config.get<bool>("read_only");
        return state;
    }

    /**
     * @brief Whether the process is in strict read-only mode.
     *
     * In strict read-only mode, the process will never attempt to become the ETL writer, and will
     * only publish ledgers as they are written to the database.
     */
    util::prometheus::Bool isStrictReadonly = PrometheusService::boolMetric(
        "read_only",
        util::prometheus::Labels{},
        "Whether the process is in strict read-only mode"
    );

    /** @brief Whether the process is writing to the database. */
    util::prometheus::Bool isWriting = PrometheusService::boolMetric(
        "etl_writing",
        util::prometheus::Labels{},
        "Whether the process is writing to the database"
    );

    /** @brief Shows whether ETL started monitor and ready to become a writer if needed */
    std::atomic_bool etlStarted{false};

    /**
     * @brief Commands for controlling the ETL writer state.
     *
     * These commands are emitted via writeCommandSignal to coordinate writer state transitions
     * across components.
     */
    enum class WriteCommand {
        StartWriting, /**< Request to attempt taking over as the ETL writer */
        StopWriting   /**< Request to give up the ETL writer role (e.g., due to write conflict) */
    };

    /**
     * @brief Signal for coordinating ETL writer state transitions.
     *
     * This signal allows components to request changes to the writer state without direct coupling.
     * - Emitted with StartWriting when database stalls and node should attempt to become writer
     * - Emitted with StopWriting when write conflicts are detected
     */
    boost::signals2::signal<void(WriteCommand)> writeCommandSignal;

    /**
     * @brief Whether clio detected an amendment block.
     *
     * Being amendment blocked means that Clio was compiled with libxrpl that does not yet support
     * some field that arrived from rippled and therefore can't extract the ledger diff. When this
     * happens, Clio can't proceed with ETL and should log this error and only handle RPC requests.
     */
    util::prometheus::Bool isAmendmentBlocked = PrometheusService::boolMetric(
        "etl_amendment_blocked",
        util::prometheus::Labels{},
        "Whether clio detected an amendment block"
    );

    /**
     * @brief Whether clio detected a corruption that needs manual attention.
     *
     * When corruption is detected, Clio should disable cache and stop the cache loading process in
     * order to prevent further corruption.
     */
    util::prometheus::Bool isCorruptionDetected = PrometheusService::boolMetric(
        "etl_corruption_detected",
        util::prometheus::Labels{},
        "Whether clio detected a corruption that needs manual attention"
    );

    /**
     * @brief Whether the cluster is using the fallback writer decision mechanism.
     *
     * The fallback mechanism is triggered when:
     * - The database stalls for 10 seconds (detected by Monitor), indicating no active writer
     * - A write conflict is detected, indicating multiple nodes attempting to write simultaneously
     *
     * When fallback mode is active, the cluster stops using the cluster communication mechanism
     * (TTL-based role announcements) and relies on the slower but more reliable database-based
     * conflict detection. This flag propagates across the cluster - if any node enters fallback
     * mode, all nodes in the cluster will switch to fallback mode.
     */
    util::prometheus::Bool isWriterDecidingFallback = PrometheusService::boolMetric(
        "etl_writing_deciding_fallback",
        util::prometheus::Labels{},
        "Whether the cluster is using the fallback writer decision mechanism"
    );
};

}  // namespace etl
