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

#include "etl/impl/SubscriptionSourceDependencies.hpp"
#include "util/Mutex.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"
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
    using OnDisconnectHook = std::function<void()>;

private:
    util::Logger log_;
    util::requests::WsConnectionBuilder wsConnectionBuilder_;
    util::requests::WsConnectionPtr wsConnection_;

    struct ValidatedLedgersData {
        std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers;
        std::string validatedLedgersRaw{"N/A"};
    };
    util::Mutex<ValidatedLedgersData> validatedLedgersData_;

    SubscriptionSourceDependencies dependencies_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    util::Retry retry_;

    OnDisconnectHook onDisconnect_;

    std::atomic_bool isConnected_{false};
    std::atomic_bool stop_{false};
    std::atomic_bool isForwarding_{false};

    util::Mutex<std::chrono::system_clock::time_point> lastMessageTime_;

    static constexpr std::chrono::seconds CONNECTION_TIMEOUT{30};
    static constexpr std::chrono::seconds RETRY_MAX_DELAY{30};
    static constexpr std::chrono::seconds RETRY_DELAY{1};

public:
    /**
     * @brief Construct a new Subscription Source object
     *
     * @tparam NetworkValidatedLedgersType The type of the network validated ledgers object
     * @tparam SubscriptionSourceType The type of the subscription source object
     * @param ioContext The io_context to use
     * @param ip The ip address of the source
     * @param wsPort The port of the source
     * @param validatedLedgers The network validated ledgers object
     * @param subscriptions The subscription source object
     * @param onDisconnect The onDisconnect hook. Called when the connection is lost
     * @param connectionTimeout The connection timeout. Defaults to 30 seconds
     * @param retryDelay The retry delay. Defaults to 1 second
     */
    template <typename NetworkValidatedLedgersType, typename SubscriptionSourceType>
    SubscriptionSource(
        boost::asio::io_context& ioContext,
        std::string const& ip,
        std::string const& wsPort,
        std::shared_ptr<NetworkValidatedLedgersType> validatedLedgers,
        std::shared_ptr<SubscriptionSourceType> subscriptions,
        OnDisconnectHook onDisconnect,
        std::chrono::milliseconds const connectionTimeout = CONNECTION_TIMEOUT,
        std::chrono::milliseconds const retryDelay = RETRY_DELAY
    )
        : log_(fmt::format("GrpcSource[{}:{}]", ip, wsPort))
        , wsConnectionBuilder_(ip, wsPort)
        , dependencies_(std::move(validatedLedgers), std::move(subscriptions))
        , strand_(boost::asio::make_strand(ioContext))
        , retry_(util::makeRetryExponentialBackoff(retryDelay, RETRY_MAX_DELAY, strand_))
        , onDisconnect_(std::move(onDisconnect))
    {
        wsConnectionBuilder_.addHeader({boost::beast::http::field::user_agent, "clio-client"})
            .addHeader({"X-User", "clio-client"})
            .setConnectionTimeout(connectionTimeout);
        subscribe();
    }

    /**
     * @brief Destroy the Subscription Source object
     *
     * @note This will block to wait for all the async operations to complete. io_context must be still running
     */
    ~SubscriptionSource();

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
     * @brief Set source forwarding
     *
     * @note If forwarding is true the source will forward messages to the subscription manager
     * @param isForwarding The new forwarding state
     */
    void
    setForwarding(bool isForwarding);

    /**
     * @brief Get the last message time (even if the last message had an error)
     *
     * @return The last message time
     */
    std::chrono::system_clock::time_point
    lastMessageTime() const;

    /**
     * @brief Get the last received raw string of the validated ledgers
     *
     * @return The validated ledgers string
     */
    std::string const&
    validatedLedgers() const;

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
