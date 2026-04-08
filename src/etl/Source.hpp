#pragma once

#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "rpc/Errors.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/uuid.hpp>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace etl {

/**
 * @brief Provides an implementation of a ETL source
 */
class SourceBase {
public:
    using OnConnectHook = std::function<void()>;
    using OnDisconnectHook = std::function<void(bool)>;
    using OnLedgerClosedHook = std::function<void()>;

    virtual ~SourceBase() = default;

    /**
     * @brief Run subscriptions loop of the source
     */
    virtual void
    run() = 0;

    /**
     * @brief Stop Source.
     * @note This method will asynchronously wait for source to be stopped.
     *
     * @param yield The coroutine context.
     */
    virtual void
    stop(boost::asio::yield_context yield) = 0;

    /**
     * @brief Check if source is connected
     *
     * @return true if source is connected; false otherwise
     */
    [[nodiscard]] virtual bool
    isConnected() const = 0;

    /**
     * @brief Set the forwarding state of the source.
     *
     * @param isForwarding Whether to forward or not
     */
    virtual void
    setForwarding(bool isForwarding) = 0;

    /**
     * @brief Represent the source as a JSON object
     *
     * @return JSON representation of the source
     */
    [[nodiscard]] virtual boost::json::object
    toJson() const = 0;

    /** @return String representation of the source (for debug) */
    [[nodiscard]] virtual std::string
    toString() const = 0;

    /**
     * @brief Check if ledger is known by this source.
     *
     * @param sequence The ledger sequence to check
     * @return true if ledger is in the range of this source; false otherwise
     */
    [[nodiscard]] virtual bool
    hasLedger(uint32_t sequence) const = 0;

    /**
     * @brief Fetch data for a specific ledger.
     *
     * This function will continuously try to fetch data for the specified ledger until the fetch
     * succeeds, the ledger is found in the database, or the server is shutting down.
     *
     * @param sequence Sequence of the ledger to fetch
     * @param getObjects Whether to get the account state diff between this ledger and the prior
     * one; defaults to true
     * @param getObjectNeighbors Whether to request object neighbors; defaults to false
     * @return A std::pair of the response status and the response itself
     */
    [[nodiscard]] virtual std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false) = 0;

    /**
     * @brief Download a ledger in full.
     *
     * @param sequence Sequence of the ledger to download
     * @param numMarkers Number of markers to generate for async calls
     * @param loader InitialLoadObserverInterface implementation
     * @return A std::pair of the data and a bool indicating whether the download was successful
     */
    virtual InitialLedgerLoadResult
    loadInitialLedger(
        uint32_t sequence,
        std::uint32_t numMarkers,
        InitialLoadObserverInterface& loader
    ) = 0;

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param forwardToRippledClientIp IP of the client forwarding this request if known
     * @param xUserValue Value of the X-User header
     * @param yield The coroutine context
     * @return Response on success or error on failure
     */
    [[nodiscard]] virtual std::expected<boost::json::object, rpc::ClioError>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        std::string_view xUserValue,
        boost::asio::yield_context yield
    ) const = 0;
};

using SourcePtr = std::unique_ptr<SourceBase>;

using SourceFactory = std::function<SourcePtr(
    util::config::ObjectView const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    std::chrono::steady_clock::duration forwardingTimeout,
    SourceBase::OnConnectHook onConnect,
    SourceBase::OnDisconnectHook onDisconnect,
    SourceBase::OnLedgerClosedHook onLedgerClosed
)>;

/**
 * @brief Create a source
 *
 * @param config The configuration to use
 * @param ioc The io_context to run on
 * @param subscriptions Subscription manager
 * @param validatedLedgers The network validated ledgers data structure
 * @param forwardingTimeout The timeout for forwarding to rippled
 * @param onConnect The hook to call on connect
 * @param onDisconnect The hook to call on disconnect
 * @param onLedgerClosed The hook to call on ledger closed. This is called when a ledger is closed
 * and the source is set as forwarding.
 * @return The created source
 */
[[nodiscard]] SourcePtr
makeSource(
    util::config::ObjectView const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    std::chrono::steady_clock::duration forwardingTimeout,
    SourceBase::OnConnectHook onConnect,
    SourceBase::OnDisconnectHook onDisconnect,
    SourceBase::OnLedgerClosedHook onLedgerClosed
);

}  // namespace etl
