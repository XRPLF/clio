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

#include "etl/SystemState.hpp"

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
     * @brief Switch the cluster to the fallback writer decision mechanism.
     *
     * This method is called when the cluster needs to transition from the cluster
     * communication mechanism to the slower but more reliable fallback mechanism.
     * Once set, this flag propagates to all nodes in the cluster through the
     * ClioNode DbRole::Fallback state.
     */
    virtual void
    setWriterDecidingFallback() = 0;

    /**
     * @brief Whether clio is still loading cache after startup.
     *
     * @return true if clio is still loading cache, false otherwise.
     */
    [[nodiscard]] virtual bool
    isLoadingCache() const = 0;

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

public:
    /**
     * @brief Construct a WriterState with the given system state.
     * @param state Shared pointer to the system state for coordination
     */
    WriterState(std::shared_ptr<SystemState> state);

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

    /**
     * @brief Whether clio is still loading cache after startup.
     *
     * @return true if clio is still loading cache, false otherwise.
     */
    bool
    isLoadingCache() const override;

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
