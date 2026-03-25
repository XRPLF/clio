#pragma once

#include "util/Taggable.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "web/ProxyIpResolver.hpp"
#include "web/interface/Concepts.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/ConnectionHandler.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace web::ng {

/**
 * @brief Web server class.
 */
class Server : public ServerTag {
public:
    /**
     * @brief Check to perform for each new client connection. The check takes client ip as input
     * and returns a Response if the check failed. Response will be sent to the client and the
     * connection will be closed.
     */
    using OnConnectCheck = std::function<std::expected<void, Response>(Connection const&)>;

    using OnIpChangeHook = impl::ConnectionHandler::OnIpChangeHook;

    /**
     * @brief Hook called when any connection disconnects
     */
    using OnDisconnectHook = impl::ConnectionHandler::OnDisconnectHook;

    /**
     * @brief A struct that holds all the hooks for the server.
     */
    struct Hooks {
        OnConnectCheck onConnectCheck;
        OnIpChangeHook onIpChangeHook;
        OnDisconnectHook onDisconnectHook;
    };

private:
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};

    std::reference_wrapper<boost::asio::io_context> ctx_;
    std::optional<boost::asio::ssl::context> sslContext_;

    util::TagDecoratorFactory tagDecoratorFactory_;

    impl::ConnectionHandler connectionHandler_;
    boost::asio::ip::tcp::endpoint endpoint_;

    OnConnectCheck onConnectCheck_;

    bool running_{false};

public:
    /**
     * @brief Construct a new Server object.
     *
     * @param ctx The boost::asio::io_context to use.
     * @param endpoint The endpoint to listen on.
     * @param sslContext The SSL context to use (optional).
     * @param processingPolicy The requests processing policy (parallel or sequential).
     * @param parallelRequestLimit The limit of requests for one connection that can be processed in
     * parallel. Only used if processingPolicy is parallel.
     * @param tagDecoratorFactory The tag decorator factory.
     * @param proxyIpResolver The client ip resolver if a request was forwarded by a proxy
     * @param maxSubscriptionSendQueueSize The maximum size of the subscription send queue.
     * @param hooks The server hooks
     */
    Server(
        boost::asio::io_context& ctx,
        boost::asio::ip::tcp::endpoint endpoint,
        std::optional<boost::asio::ssl::context> sslContext,
        ProcessingPolicy processingPolicy,
        std::optional<size_t> parallelRequestLimit,
        util::TagDecoratorFactory tagDecoratorFactory,
        ProxyIpResolver proxyIpResolver,
        std::optional<size_t> maxSubscriptionSendQueueSize,
        Hooks hooks
    );

    /**
     * @brief Copy constructor is deleted. The Server couldn't be copied.
     */
    Server(Server const&) = delete;

    /**
     * @brief Move constructor is deleted because connectionHandler_ contains references to some
     * fields of the Server.
     */
    Server(Server&&) = delete;

    /**
     * @brief Set handler for GET requests.
     * @note This method can't be called after run() is called.
     *
     * @param target The target of the request.
     * @param handler The handler to set.
     */
    void
    onGet(std::string const& target, MessageHandler handler);

    /**
     * @brief Set handler for POST requests.
     * @note This method can't be called after run() is called.
     *
     * @param target The target of the request.
     * @param handler The handler to set.
     */
    void
    onPost(std::string const& target, MessageHandler handler);

    /**
     * @brief Set handler for WebSocket requests.
     * @note This method can't be called after run() is called.
     *
     * @param handler The handler to set.
     */
    void
    onWs(MessageHandler handler);

    /**
     * @brief Run the server.
     *
     * @return std::nullopt if the server started successfully, otherwise an error message.
     */
    std::optional<std::string>
    run();

    /**
     * @brief Stop the server. This method will asynchronously sleep unless all the users are
     * disconnected.
     * @note Stopping the server cause graceful shutdown of all connections. And rejecting new
     * connections.
     *
     * @param yield The coroutine context.
     */
    void
    stop(boost::asio::yield_context yield);

private:
    void
    handleConnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield);
};

/**
 * @brief Create a new Server.
 *
 * @param config The configuration.
 * @param onConnectCheck The check to perform on each client connection.
 * @param onDisconnectHook The hook to call when client disconnects.
 * @param context The boost::asio::io_context to use.
 *
 * @return The Server or an error message.
 */
std::expected<Server, std::string>
makeServer(
    util::config::ClioConfigDefinition const& config,
    Server::OnConnectCheck onConnectCheck,
    Server::OnIpChangeHook onIpChangeHook,
    Server::OnDisconnectHook onDisconnectHook,
    boost::asio::io_context& context
);

}  // namespace web::ng
