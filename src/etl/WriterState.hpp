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
    std::shared_ptr<SystemState> systemState_; /**< @brief Shared system state for ETL coordination */

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

    std::unique_ptr<WriterStateInterface>
    clone() const override;
};

}  // namespace etl
