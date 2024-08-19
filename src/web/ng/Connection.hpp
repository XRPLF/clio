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

#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>

namespace web::ng {

class ConnectionContext;

class Connection {
    size_t id_;

public:
    Connection();

    virtual ~Connection() = default;

    virtual void
    send(Response response, boost::asio::yield_context yield) = 0;

    virtual std::expected<Request, RequestError>
    receive(boost::asio::yield_context yield) = 0;

    virtual void
    close(std::chrono::steady_clock::duration timeout) = 0;

    void
    subscribeToDisconnect();

    ConnectionContext
    context() const;

    size_t
    id() const;

    struct Hash {
        size_t
        operator()(Connection const& connection) const;
    };
};

class ConnectionContext {
    std::reference_wrapper<Connection> connection_;

public:
    explicit ConnectionContext(Connection& connection);

    ConnectionContext(ConnectionContext&&) = delete;
    ConnectionContext(ConnectionContext const&) = default;

    bool
    isAdmin() const;
};

using ConnectionPtr = std::unique_ptr<Connection>;

}  // namespace web::ng
