#pragma once

#include "util/StopHelper.hpp"
#include "util/StringHash.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "web/ProxyIpResolver.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace web::ng::impl {

class ConnectionHandler {
public:
    using OnDisconnectHook = std::function<void(Connection const&)>;
    using OnIpChangeHook = std::function<void(std::string const&, std::string const&)>;
    using TargetToHandlerMap =
        std::unordered_map<std::string, MessageHandler, util::StringHash, std::equal_to<>>;

private:
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};

    ProcessingPolicy processingPolicy_;
    std::optional<size_t> maxParallelRequests_;

    std::reference_wrapper<util::TagDecoratorFactory> tagFactory_;
    std::optional<size_t> maxSubscriptionSendQueueSize_;

    ProxyIpResolver proxyIpResolver_;

    OnDisconnectHook onDisconnectHook_;
    OnIpChangeHook onIpChangeHook_;

    TargetToHandlerMap getHandlers_;
    TargetToHandlerMap postHandlers_;
    std::optional<MessageHandler> wsHandler_;

    boost::signals2::signal<void()> onStop_;
    std::unique_ptr<std::atomic_bool> stopping_ = std::make_unique<std::atomic_bool>(false);

    std::reference_wrapper<util::prometheus::GaugeInt> connectionsCounter_ =
        PrometheusService::gaugeInt(
            "connections_total_number",
            util::prometheus::Labels{{{"status", "connected"}}}
        );

    util::StopHelper stopHelper_;

public:
    ConnectionHandler(
        ProcessingPolicy processingPolicy,
        std::optional<size_t> maxParallelRequests,
        util::TagDecoratorFactory& tagFactory,
        std::optional<size_t> maxSubscriptionSendQueueSize,
        ProxyIpResolver proxyIpResolver,
        OnDisconnectHook onDisconnectHook,
        OnIpChangeHook onIpChangeHook
    );

    ConnectionHandler(ConnectionHandler&&) = delete;

    static constexpr std::chrono::milliseconds kCloseConnectionTimeout{500};

    void
    onGet(std::string const& target, MessageHandler handler);

    void
    onPost(std::string const& target, MessageHandler handler);

    void
    onWs(MessageHandler handler);

    void
    processConnection(ConnectionPtr connection, boost::asio::yield_context yield);

    static void
    stopConnection(Connection& connection, boost::asio::yield_context yield);

    void
    stop(boost::asio::yield_context yield);

    [[nodiscard]] bool
    isStopping() const;

private:
    /**
     * @brief Handle an error.
     *
     * @param error The error to handle.
     * @param connection The connection that caused the error.
     * @return True if the connection should be gracefully closed, false otherwise.
     */
    [[nodiscard]] bool
    handleError(Error const& error, Connection const& connection) const;

    /**
     * @brief The request-response loop.
     *
     * @param connection The connection to handle.
     * @param yield The yield context.
     * @return True if the connection should be gracefully closed, false otherwise.
     */
    bool
    sequentRequestResponseLoop(
        Connection& connection,
        SubscriptionContextPtr& subscriptionContext,
        boost::asio::yield_context yield
    );

    bool
    parallelRequestResponseLoop(
        Connection& connection,
        SubscriptionContextPtr& subscriptionContext,
        boost::asio::yield_context yield
    );

    std::optional<bool>
    processRequest(
        Connection& connection,
        SubscriptionContextPtr& subscriptionContext,
        Request const& request,
        boost::asio::yield_context yield
    );

    Response
    handleRequest(
        ConnectionMetadata& connectionMetadata,
        SubscriptionContextPtr& subscriptionContext,
        Request const& request,
        boost::asio::yield_context yield
    );

    void
    resolveClientIp(Connection& connection, Request const& request) const;
};

}  // namespace web::ng::impl
