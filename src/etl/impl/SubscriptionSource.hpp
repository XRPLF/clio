//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/Source.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/Mutex.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/field.hpp>
#include <fmt/core.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

/**
 * @brief This class is used to subscribe to a source of ledger data and forward it to the subscription manager.
 */
class SubscriptionSource {
public:
    using OnConnectHook = SourceBase::OnConnectHook;
    using OnDisconnectHook = SourceBase::OnDisconnectHook;
    using OnLedgerClosedHook = SourceBase::OnLedgerClosedHook;

private:
    util::Logger log_;
    util::requests::WsConnectionBuilder wsConnectionBuilder_;
    util::requests::WsConnectionPtr wsConnection_;

    struct ValidatedLedgersData {
        std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers;
        std::string validatedLedgersRaw{"N/A"};
    };
    util::Mutex<ValidatedLedgersData> validatedLedgersData_;

    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers_;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    std::chrono::steady_clock::duration wsTimeout_;

    util::Retry retry_;

    OnConnectHook onConnect_;
    OnDisconnectHook onDisconnect_;
    OnLedgerClosedHook onLedgerClosed_;

    std::atomic_bool isConnected_{false};
    std::atomic_bool stop_{false};
    std::atomic_bool isForwarding_{false};

    util::Mutex<std::chrono::steady_clock::time_point> lastMessageTime_;

    std::reference_wrapper<util::prometheus::GaugeInt> lastMessageTimeSecondsSinceEpoch_;

    std::future<void> runFuture_;

    static constexpr std::chrono::seconds WS_TIMEOUT{30};
    static constexpr std::chrono::seconds RETRY_MAX_DELAY{30};
    static constexpr std::chrono::seconds RETRY_DELAY{1};

public:
    /**
     * @brief Construct a new Subscription Source object
     *
     * @tparam NetworkValidatedLedgersType The type of the network validated ledgers object
     * @param ioContext The io_context to use
     * @param ip The ip address of the source
     * @param wsPort The port of the source
     * @param validatedLedgers The network validated ledgers object
     * @param subscriptions The subscription manager object
     * @param onDisconnect The onDisconnect hook. Called when the connection is lost
     * @param onNewLedger The onNewLedger hook. Called when a new ledger is received
     * @param onLedgerClosed The onLedgerClosed hook. Called when the ledger is closed but only if the source is
     * forwarding
     * @param wsTimeout A timeout for websocket operations. Defaults to 30 seconds
     * @param retryDelay The retry delay. Defaults to 1 second
     */
    SubscriptionSource(
        boost::asio::io_context& ioContext,
        std::string const& ip,
        std::string const& wsPort,
        std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        OnConnectHook onConnect,
        OnDisconnectHook onDisconnect,
        OnLedgerClosedHook onLedgerClosed,
        std::chrono::steady_clock::duration const wsTimeout = WS_TIMEOUT,
        std::chrono::steady_clock::duration const retryDelay = RETRY_DELAY
    );

    /**
     * @brief Destroy the Subscription Source object
     *
     * @note This will block to wait for all the async operations to complete. io_context must be still running
     */
    ~SubscriptionSource();

    /**
     * @brief Run the source
     */
    void
    run();

    /**
     * @brief Check if the source has a ledger
     *
     * @param sequence The sequence of the ledger
     * @return true if the source has the ledger, false otherwise
     */
    bool
    hasLedger(uint32_t sequence) const;

    /**
     * @brief Check if the source is connected
     *
     * @return true if the source is connected, false otherwise
     */
    bool
    isConnected() const;

    /**
     * @brief Get whether the source is forwarding
     *
     * @return true if the source is forwarding, false otherwise
     */
    bool
    isForwarding() const;

    /**
     * @brief Set source forwarding
     *
     * @note If forwarding is true the source will forward messages to the subscription manager. Forwarding is being
     * reset on disconnect.
     * @param isForwarding The new forwarding state
     */
    void
    setForwarding(bool isForwarding);

    /**
     * @brief Get the last message time (even if the last message had an error)
     *
     * @return The last message time
     */
    std::chrono::steady_clock::time_point
    lastMessageTime() const;

    /**
     * @brief Get the last received raw string of the validated ledgers
     *
     * @return The validated ledgers string
     */
    std::string const&
    validatedRange() const;

    /**
     * @brief Stop the source. The source will complete already scheduled operations but will not schedule new ones
     */
    void
    stop();

private:
    void
    subscribe();

    std::optional<util::requests::RequestError>
    handleMessage(std::string const& message);

    void
    handleError(util::requests::RequestError const& error, boost::asio::yield_context yield);

    void
    logError(util::requests::RequestError const& error) const;

    void
    setLastMessageTime();

    void
    setValidatedRange(std::string range);

    static std::string const&
    getSubscribeCommandJson();
};

}  // namespace etl::impl
