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

#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <chrono>
#include <cstdint>

namespace etl {

/**
 * @brief An interface for the monitor service
 * An implementation of this service is responsible for periodically checking various datasources to detect newly
 * ingested ledgers.
 */
class MonitorInterface {
public:
    static constexpr auto kDEFAULT_REPEAT_INTERVAL = std::chrono::seconds{1};
    using NewSequenceSignalType = boost::signals2::signal<void(uint32_t)>;
    using DbStalledSignalType = boost::signals2::signal<void()>;

    virtual ~MonitorInterface() = default;

    /**
     * @brief Allows the loading process to notify of a freshly committed ledger
     * @param seq The ledger sequence loaded
     */
    virtual void
    notifySequenceLoaded(uint32_t seq) = 0;

    /**
     * @brief Notifies the monitor of a write conflict
     * @param seq The sequence number of the ledger that encountered a write conflict
     */
    virtual void
    notifyWriteConflict(uint32_t seq) = 0;

    /**
     * @brief Allows clients to get notified when a new ledger becomes available in Clio's database
     *
     * @param subscriber The slot to connect
     * @return A connection object that automatically disconnects the subscription once destroyed
     */
    [[nodiscard]] virtual boost::signals2::scoped_connection
    subscribeToNewSequence(NewSequenceSignalType::slot_type const& subscriber) = 0;

    /**
     * @brief Allows clients to get notified when no database update is detected for a configured period.
     *
     * @param subscriber The slot to connect
     * @return A connection object that automatically disconnects the subscription once destroyed
     */
    [[nodiscard]] virtual boost::signals2::scoped_connection
    subscribeToDbStalled(DbStalledSignalType::slot_type const& subscriber) = 0;

    /**
     * @brief Run the monitor service
     *
     * @param repeatInterval The interval between attempts to check the database for new ledgers
     */
    virtual void
    run(std::chrono::steady_clock::duration repeatInterval = kDEFAULT_REPEAT_INTERVAL) = 0;

    /**
     * @brief Stops the monitor service
     */
    virtual void
    stop() = 0;
};

}  // namespace etl
