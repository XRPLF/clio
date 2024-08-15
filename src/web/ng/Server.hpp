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

#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/ng/MessageHandler.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace web::ng {

class Server {
    util::Logger log_{"WebServer"};
    boost::asio::io_context& ctx_;
    std::unique_ptr<dosguard::DOSGuardInterface> dosguard_;

    std::unordered_map<std::string, MessageHandler> getHandlers_;
    std::unordered_map<std::string, MessageHandler> postHandlers_;
    std::optional<MessageHandler> wsHandler_;

    boost::asio::ip::tcp::endpoint endpoint_;

    std::future<void> running_;

public:
    Server(
        util::Config const& config,
        std::unique_ptr<dosguard::DOSGuardInterface> dosguard,
        boost::asio::io_context& ctx
    );

    Server(Server const&) = delete;
    Server(Server&&) = delete;

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
    makeConnnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield);
};

}  // namespace web::ng
