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
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"
#include "web/ng/MessageHandler.hpp"
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
class Server {
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};
    std::reference_wrapper<boost::asio::io_context> ctx_;

    std::optional<boost::asio::ssl::context> sslContext_;

    impl::ConnectionHandler connectionHandler_;

    boost::asio::ip::tcp::endpoint endpoint_;

    util::TagDecoratorFactory tagDecoratorFactory_;

    bool running_{false};

public:
    /**
     * @brief Construct a new Server object.
     *
     * @param ctx The boost::asio::io_context to use.
     * @param endpoint The endpoint to listen on.
     * @param sslContext The SSL context to use (optional).
     * @param connectionHandler The connection handler.
     * @param tagDecoratorFactory The tag decorator factory.
     */
    Server(
        boost::asio::io_context& ctx,
        boost::asio::ip::tcp::endpoint endpoint,
        std::optional<boost::asio::ssl::context> sslContext,
        impl::ConnectionHandler connectionHandler,
        util::TagDecoratorFactory tagDecoratorFactory
    );

    /**
     * @brief Copy constructor is deleted. The Server couldn't be copied.
     */
    Server(Server const&) = delete;

    /**
     * @brief Move constructor is defaulted.
     */
    Server(Server&&) = default;

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
     * @brief Stop the server.
     ** @note Stopping the server cause graceful shutdown of all connections. And rejecting new connections.
     */
    void
    stop();

private:
    void
    handleConnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield);
};

/**
 * @brief Create a new Server.
 *
 * @param config The configuration.
 * @param context The boost::asio::io_context to use.
 *
 * @return The Server or an error message.
 */
std::expected<Server, std::string>
make_Server(util::Config const& config, boost::asio::io_context& context);

}  // namespace web::ng
