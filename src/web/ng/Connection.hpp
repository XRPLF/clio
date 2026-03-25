#pragma once

#include "util/Taggable.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng {

/**
 * @brief An interface for a connection metadata class.
 */
class ConnectionMetadata : public util::Taggable {
protected:
    std::string ip_;  // client ip
    std::optional<bool> isAdmin_;

public:
    /**
     * @brief Construct a new ConnectionMetadata object.
     *
     * @param ip The client ip.
     * @param tagDecoratorFactory The factory for creating tag decorators.
     */
    ConnectionMetadata(std::string ip, util::TagDecoratorFactory const& tagDecoratorFactory);

    /**
     * @brief Whether the connection was upgraded. Upgraded connections are websocket connections.
     *
     * @return true if the connection was upgraded.
     */
    virtual bool
    wasUpgraded() const = 0;

    /**
     * @brief Get the ip of the client.
     *
     * @return The ip of the client.
     */
    std::string const&
    ip() const;

    /**
     * @brief Set the ip of the client.
     *
     * @param newIp The new ip to set.
     */
    void
    setIp(std::string newIp)
    {
        ip_ = std::move(newIp);
    }

    /**
     * @brief Get whether the client is an admin.
     *
     * @return true if the client is an admin.
     */
    bool
    isAdmin() const;

    /**
     * @brief Set the isAdmin field.
     * @note This function is lazy, it will update isAdmin only if it is not set yet.
     *
     * @tparam T The invocable type of the function to call to set the isAdmin.
     * @param setter The function to call to set the isAdmin.
     */
    template <std::invocable T>
    void
    setIsAdmin(T&& setter)
    {
        if (not isAdmin_.has_value())
            isAdmin_ = setter();
    }
};

/**
 * @brief A class representing a connection to a client.
 */
class Connection : public ConnectionMetadata {
protected:
    boost::beast::flat_buffer buffer_;

public:
    /**
     * @brief The default timeout for send, receive, and close operations.
     * @note This value should be higher than forwarding timeout to not disconnect clients if
     * rippled is slow.
     */
    static constexpr std::chrono::steady_clock::duration kDEFAULT_TIMEOUT =
        std::chrono::seconds{11};

    /**
     * @brief Construct a new Connection object
     *
     * @param ip The client ip.
     * @param buffer The buffer to use for reading and writing.
     * @param tagDecoratorFactory The factory for creating tag decorators.
     */
    Connection(
        std::string ip,
        boost::beast::flat_buffer buffer,
        util::TagDecoratorFactory const& tagDecoratorFactory
    );

    /**
     * @brief Get the timeout for send, receive, and close operations. For WebSocket connections,
     * this is the ping interval.
     *
     * @param newTimeout The new timeout to set.
     */
    virtual void
    setTimeout(std::chrono::steady_clock::duration newTimeout) = 0;

    /**
     * @brief Send a response to the client.
     *
     * @param response The response to send.
     * @param yield The yield context.
     * @return An error if the operation fails, otherwise nothing.
     */
    virtual std::expected<void, Error>
    send(Response response, boost::asio::yield_context yield) = 0;

    /**
     * @brief Receive a request from the client.
     *
     * @param yield The yield context.
     * @return The request if it was received or an error if the operation failed.
     */
    virtual std::expected<Request, Error>
    receive(boost::asio::yield_context yield) = 0;

    /**
     * @brief Gracefully close the connection.
     *
     * @param yield The yield context.
     */
    virtual void
    close(boost::asio::yield_context yield) = 0;
};

/**
 * @brief A pointer to a connection.
 */
using ConnectionPtr = std::unique_ptr<Connection>;

}  // namespace web::ng
