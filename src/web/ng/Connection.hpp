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

#include "util/Taggable.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace web::ng {

/**
 * @brief A forward declaration of ConnectionContext.
 */
class ConnectionContext;

/**
 *@brief A class representing a connection to a client.
 */
class Connection : public util::Taggable {
protected:
    std::string ip_;  // client ip
    boost::beast::flat_buffer buffer_;

public:
    /**
     * @brief The default timeout for send, receive, and close operations.
     */
    static constexpr std::chrono::steady_clock::duration DEFAULT_TIMEOUT = std::chrono::seconds{30};

    /**
     * @brief Construct a new Connection object
     *
     * @param ip The client ip.
     * @param buffer The buffer to use for reading and writing.
     * @param tagDecoratorFactory The factory for creating tag decorators.
     */
    Connection(std::string ip, boost::beast::flat_buffer buffer, util::TagDecoratorFactory const& tagDecoratorFactory);

    /**
     * @brief Whether the connection was upgraded. Upgraded connections are websocket connections.
     *
     * @return true if the connection was upgraded.
     */
    virtual bool
    wasUpgraded() const = 0;

    /**
     * @brief Send a response to the client.
     *
     * @param response The response to send.
     * @param yield The yield context.
     * @param timeout The timeout for the operation.
     * @return An error if the operation failed or nullopt if it succeeded.
     */

    virtual std::optional<Error>
    send(
        Response response,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT
    ) = 0;

    /**
     * @brief Receive a request from the client.
     *
     * @param yield The yield context.
     * @param timeout The timeout for the operation.
     * @return The request if it was received or an error if the operation failed.
     */
    virtual std::expected<Request, Error>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) = 0;

    /**
     * @brief Gracefully close the connection.
     *
     * @param yield The yield context.
     * @param timeout The timeout for the operation.
     */
    virtual void
    close(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = DEFAULT_TIMEOUT) = 0;

    /**
     * @brief Get the connection context.
     *
     * @return The connection context.
     */
    ConnectionContext
    context() const;

    /**
     * @brief Get the ip of the client.
     *
     * @return The ip of the client.
     */
    std::string const&
    ip() const;
};

/**
 * @brief A pointer to a connection.
 */
using ConnectionPtr = std::unique_ptr<Connection>;

/**
 * @brief A class representing the context of a connection.
 */
class ConnectionContext {
    std::reference_wrapper<Connection const> connection_;

public:
    /**
     * @brief Construct a new ConnectionContext object.
     *
     * @param connection The connection.
     */
    explicit ConnectionContext(Connection const& connection);
};

}  // namespace web::ng
