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

namespace Server {

namespace http = boost::beast::http;

/**
 * @brief Base class for all connections
 * This class is used to represent a connection in RPC executor and subscription manager
 */
struct ConnectionBase : public util::Taggable
{
protected:
    boost::system::error_code ec_;

public:
    std::string const clientIp;
    bool upgraded = false;

    ConnectionBase(util::TagDecoratorFactory const& tagFactory, std::string ip) : Taggable(tagFactory), clientIp(ip)
    {
    }

    /**
     * @brief Send the response to the client
     * @param msg The message to send
     */
    virtual void
    send(std::string&& msg, http::status status = http::status::ok) = 0;

    /**
     * @brief Send via shared_ptr of string, that enables SubscriptionManager to publish to clients
     * @param msg The message to send
     */
    virtual void
    send(std::shared_ptr<std::string> msg)
    {
        throw std::runtime_error("web server can not send the shared payload");
    }

    /**
     * @brief Indicates whether the connection had an error and is considered
     * dead
     *
     * @return true
     * @return false
     */
    bool
    dead()
    {
        return ec_ != boost::system::error_code{};
    }

    virtual ~ConnectionBase() = default;
};
}  // namespace Server
