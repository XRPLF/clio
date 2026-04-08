#pragma once

#include "data/LedgerCacheInterface.hpp"
#include "etl/SystemState.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <functional>
#include <memory>

namespace etl {

/**
 * @brief Interface for managing writer state in the ETL subsystem.
 *
 * This interface provides methods to query and control whether the ETL process
 * is actively writing to the database. Implementations should coordinate with
 * the ETL system state to manage write responsibilities.
 */
class WriterStateInterface {
public:
    virtual ~WriterStateInterface() = default;

    /**
     * @brief Check if the ETL process is in strict read-only mode.
     * @return true if the process is in strict read-only mode, false otherwise
     */
    [[nodiscard]] virtual bool
    isReadOnly() const = 0;

    /**
     * @brief Check if the ETL process is currently writing to the database.
     * @return true if the process is writing, false otherwise
     */
    [[nodiscard]] virtual bool
    isWriting() const = 0;

    /**
     * @brief Request to start writing to the database.
     *
     * This method signals that the process should take over writing responsibilities.
     * The actual transition to writing state may not be immediate.
     */
    virtual void
    startWriting() = 0;

    /**
     * @brief Request to stop writing to the database.
     *
     * This method signals that the process should give up writing responsibilities.
     * The actual transition from writing state may not be immediate.
     */
    virtual void
    giveUpWriting() = 0;

    /**
     * @brief Check if the cluster is using the fallback writer decision mechanism.
     *
     * @return true if the cluster has switched to fallback mode, false otherwise
     */
    [[nodiscard]] virtual bool
    isFallback() const = 0;

    /**
     * @brief Check if this node is in fallback recovery mode.
     *
     * Fallback recovery is an intermediate state entered when the node has been in
     * fallback mode long enough to attempt returning to election-based writer selection.
     * In this state the node continues participating in the fallback write-race while
     * coordinating with other nodes to exit fallback together.
     *
     * @return true if the node is in fallback recovery mode, false otherwise
     */
    [[nodiscard]] virtual bool
    isFallbackRecovery() const = 0;

    /**
     * @brief Set or clear the fallback recovery flag.
     *
     * When @p newValue is true, the node enters fallback recovery mode:
     *   - @ref isFallbackRecovery returns true
     *   - The plain fallback flag (@ref isFallback) is cleared so the node no longer
     *     publishes @c DbRole::Fallback; it publishes @c DbRole::FallbackRecovery instead.
     *
     * When @p newValue is false, the recovery flag is cleared without touching the
     * plain fallback flag.  This is used when the recovery coordination completes and
     * the node transitions back to election mode.
     *
     * @param newValue true to enter recovery mode, false to leave it
     */
    virtual void
    setFallbackRecovery(bool newValue) = 0;

    /**
     * @brief Switch the cluster to the fallback writer decision mechanism.
     *
     * This method is called when the cluster needs to transition from the cluster
     * communication mechanism to the slower but more reliable fallback mechanism.
     * Once set, this flag propagates to all nodes in the cluster through the
     * ClioNode DbRole::Fallback state.
     *
     * Also clears the fallback recovery flag (@ref isFallbackRecovery) because entering
     * a fresh fallback period cancels any in-progress recovery attempt.
     */
    virtual void
    setWriterDecidingFallback() = 0;

    /**
     * @brief Whether the ETL monitor has started and the node is ready to become a writer.
     *
     * @return true if ETL has started the monitor loop, false otherwise.
     */
    [[nodiscard]] virtual bool
    isEtlStarted() const = 0;

    /**
     * @brief Whether the ledger cache is fully loaded.
     *
     * @return true if the cache is full, false otherwise.
     */
    [[nodiscard]] virtual bool
    isCacheFull() const = 0;

    /**
     * @brief Create a clone of this writer state.
     *
     * Creates a new instance of the writer state with the same underlying system state.
     * This is used when spawning operations that need their own writer state instance
     * while sharing the same system state.
     *
     * @return A unique pointer to the cloned writer state.
     */
    [[nodiscard]] virtual std::unique_ptr<WriterStateInterface>
    clone() const = 0;
};

/**
 * @brief Implementation of WriterStateInterface that manages ETL writer state.
 *
 * This class coordinates with SystemState to manage whether the ETL process
 * is actively writing to the database. It provides methods to query the current
 * writing state and request transitions between writing and non-writing states.
 */
class WriterState : public WriterStateInterface {
private:
    std::shared_ptr<SystemState>
        systemState_; /**< @brief Shared system state for ETL coordination */
    std::reference_wrapper<data::LedgerCacheInterface const> cache_;

    /**
     * @brief Prometheus metric tracking whether this node is in fallback recovery mode.
     *
     * @note Because @c prometheus::Bool holds a @c std::reference_wrapper to the underlying
     * gauge, copies of @c WriterState (including clones) share the same metric value.
     * Mutations made through a clone are therefore immediately visible on the original
     * instance and vice-versa.
     */
    util::prometheus::Bool isFallbackRecovery_ = PrometheusService::boolMetric(
        "etl_writing_deciding_fallback_recovery",
        util::prometheus::Labels{},
        "Whether clio is in recovery from the fallback writer decision mechanism"
    );

public:
    /**
     * @brief Construct a WriterState with the given system state and cache.
     *
     * @param state Shared pointer to the system state for coordination
     * @param cache The ledger cache used to report cache fullness
     */
    WriterState(std::shared_ptr<SystemState> state, data::LedgerCacheInterface const& cache);

    bool
    isReadOnly() const override;

    /**
     * @brief Check if the ETL process is currently writing to the database.
     * @return true if the process is writing, false otherwise
     */
    bool
    isWriting() const override;

    /**
     * @brief Request to start writing to the database.
     *
     * If already writing, this method does nothing. Otherwise, it sets the
     * shouldTakeoverWriting flag in the system state to signal the request.
     */
    void
    startWriting() override;

    /**
     * @brief Request to stop writing to the database.
     *
     * If not currently writing, this method does nothing. Otherwise, it sets the
     * shouldGiveUpWriter flag in the system state to signal the request.
     */
    void
    giveUpWriting() override;

    /**
     * @brief Switch the cluster to the fallback writer decision mechanism.
     *
     * Sets the isWriterDecidingFallback flag in the system state, which will be
     * propagated to other nodes in the cluster through the ClioNode DbRole::Fallback state.
     */
    void
    setWriterDecidingFallback() override;

    /**
     * @brief Check if the cluster is using the fallback writer decision mechanism.
     *
     * @return true if the cluster has switched to fallback mode, false otherwise
     */
    bool
    isFallback() const override;

    /** @copydoc WriterStateInterface::isFallbackRecovery */
    bool
    isFallbackRecovery() const override;

    /** @copydoc WriterStateInterface::setFallbackRecovery */
    void
    setFallbackRecovery(bool newValue) override;

    /** @copydoc WriterStateInterface::isEtlStarted */
    bool
    isEtlStarted() const override;

    /** @copydoc WriterStateInterface::isCacheFull */
    bool
    isCacheFull() const override;

    /**
     * @brief Create a clone of this writer state.
     *
     * Creates a new WriterState instance sharing the same system state.
     *
     * @return A unique pointer to the cloned writer state.
     */
    std::unique_ptr<WriterStateInterface>
    clone() const override;
};

}  // namespace etl
