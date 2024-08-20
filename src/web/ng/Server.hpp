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

#include "util/Mutex.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace web::ng {

class Server {
    util::Logger log_{"WebServer"};
    std::reference_wrapper<boost::asio::io_context> ctx_;

    std::unique_ptr<dosguard::DOSGuardInterface> dosguard_;
    std::shared_ptr<web::impl::AdminVerificationStrategy> adminVerificationStrategy_;
    std::optional<boost::asio::ssl::context> sslContext_;

    std::unordered_map<std::string, MessageHandler> getHandlers_;
    std::unordered_map<std::string, MessageHandler> postHandlers_;
    std::optional<MessageHandler> wsHandler_;

    using ConnectionsSet = std::unordered_set<ConnectionPtr, Connection::Hash>;
    std::unique_ptr<util::Mutex<ConnectionsSet, std::shared_mutex>> connections_;

    boost::asio::ip::tcp::endpoint endpoint_;

public:
    Server(
        boost::asio::io_context& ctx,
        boost::asio::ip::tcp::endpoint endpoint,
        std::optional<boost::asio::ssl::context> sslContext,
        std::shared_ptr<web::impl::AdminVerificationStrategy> adminVerificationStrategy,
        std::unique_ptr<dosguard::DOSGuardInterface> dosguard
    );

    Server(Server const&) = delete;
    Server(Server&&) = default;

    std::optional<std::string>
    run();

    void
    onGet(std::string const& target, MessageHandler handler);

    void
    onPost(std::string const& target, MessageHandler handler);

    void
    onWs(MessageHandler handler);

    void
    stop();

private:
    void
    makeConnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield);

    void
    handleConnection(Connection& connection, boost::asio::yield_context yield);

    void
    handleConnectionLoop(Connection& connection, boost::asio::yield_context yield);

    Response
    handleRequest(Request request, ConnectionContext connectionContext);
};

std::expected<Server, std::string>
make_Server(
    util::Config const& config,
    boost::asio::io_context& context,
    std::unique_ptr<dosguard::DOSGuardInterface> dosguard
);

}  // namespace web::ng
