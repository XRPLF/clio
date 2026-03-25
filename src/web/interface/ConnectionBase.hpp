#pragma once

#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/signals2.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace web {

namespace http = boost::beast::http;

/**
 * @brief Base class for all connections.
 *
 * This class is used to represent a connection in RPC executor and subscription manager.
 */
struct ConnectionBase : public util::Taggable {
protected:
    boost::system::error_code ec_;
    bool isAdmin_ = false;
    std::string clientIp_;

public:
    bool upgraded = false;

    /**
     * @brief Create a new connection base.
     *
     * @param tagFactory The factory that generates tags to track sessions and requests
     * @param ip The IP address of the connected peer
     */
    ConnectionBase(util::TagDecoratorFactory const& tagFactory, std::string ip)
        : Taggable(tagFactory), clientIp_(std::move(ip))
    {
    }

    /**
     * @brief Send the response to the client.
     *
     * @param msg The message to send
     * @param status The HTTP status code; defaults to OK
     */
    virtual void
    send(std::string&& msg, http::status status = http::status::ok) = 0;

    /**
     * @brief Send via shared_ptr of string, that enables SubscriptionManager to publish to clients.
     *
     * @param msg Unused
     * @throws std::logic_error unless the function is overridden by a child class.
     */
    virtual void
    send([[maybe_unused]] std::shared_ptr<std::string> msg)
    {
        throw std::logic_error("web server can not send the shared payload");
    }

    /**
     * @brief Send a "slow down" error response to the client.
     *
     * @param request The original request that triggered the rate limiting
     */
    virtual void
    sendSlowDown(std::string const& request) = 0;
    /**
     * @brief Get the subscription context for this connection.
     *
     * @param factory Tag TagDecoratorFactory to use to create the context.
     * @return The subscription context for this connection.
     */
    virtual SubscriptionContextPtr
    makeSubscriptionContext(util::TagDecoratorFactory const& factory) = 0;

    /**
     * @brief Indicates whether the connection had an error and is considered dead.
     *
     * @return true if the connection is considered dead; false otherwise
     */
    bool
    dead()
    {
        return ec_ != boost::system::error_code{};
    }

    /**
     * @brief Indicates whether the connection has admin privileges
     *
     * @return true if the connection is from admin user
     */
    [[nodiscard]] bool
    isAdmin() const
    {
        return isAdmin_;
    }

    /**
     * @brief Get the IP address of the client.
     *
     * @return The IP address of the client.
     */
    [[nodiscard]] std::string const&
    clientIp() const
    {
        return clientIp_;
    }
};
}  // namespace web
