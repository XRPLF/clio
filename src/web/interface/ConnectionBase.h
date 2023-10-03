//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <util/Taggable.h>

#include <boost/beast/http.hpp>
#include <utility>

namespace web {

namespace http = boost::beast::http;

/**
 * @brief Base class for all connections.
 *
 * This class is used to represent a connection in RPC executor and subscription manager.
 */
struct ConnectionBase : public util::Taggable
{
protected:
    boost::system::error_code ec_;

public:
    std::string const clientIp;
    bool upgraded = false;
    bool isAdmin_ = false;

    /**
     * @brief Create a new connection base.
     *
     * @param tagFactory The factory that generates tags to track sessions and requests
     * @param ip The IP address of the connected peer
     */
    ConnectionBase(util::TagDecoratorFactory const& tagFactory, std::string ip)
        : Taggable(tagFactory), clientIp(std::move(ip))
    {
    }

    ~ConnectionBase() override = default;

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
     * @param msg The message to send
     * @throws Not supported unless implemented in child classes. Will always throw std::logic_error.
     */
    virtual void send(std::shared_ptr<std::string> /* msg */)
    {
        throw std::logic_error("web server can not send the shared payload");
    }

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
};
}  // namespace web
