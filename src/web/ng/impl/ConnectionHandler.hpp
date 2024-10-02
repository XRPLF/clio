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
#include "util/log/Logger.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace web::ng::impl {

class ConnectionHandler {
public:
    enum class ProcessingStrategy { Sequent, Parallel };

    struct StringHash {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;

        std::size_t
        operator()(char const* str) const;
        std::size_t
        operator()(std::string_view str) const;
        std::size_t
        operator()(std::string const& str) const;
    };

    using TargetToHandlerMap = std::unordered_map<std::string, MessageHandler, StringHash, std::equal_to<>>;

private:
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};

    ProcessingStrategy processingStrategy_;
    std::optional<size_t> maxParallelRequests_;

    TargetToHandlerMap getHandlers_;
    TargetToHandlerMap postHandlers_;
    std::optional<MessageHandler> wsHandler_;

    using ConnectionsMap = std::unordered_map<size_t, ConnectionPtr>;
    std::unique_ptr<util::Mutex<ConnectionsMap>> connections_{std::make_unique<util::Mutex<ConnectionsMap>>()};

public:
    ConnectionHandler(ProcessingStrategy processingStrategy, std::optional<size_t> maxParallelRequests_);

    void
    onGet(std::string const& target, MessageHandler handler);

    void
    onPost(std::string const& target, MessageHandler handler);

    void
    onWs(MessageHandler handler);

    void
    processConnection(ConnectionPtr connection, boost::asio::yield_context yield);

private:
    /**
     * @brief Insert a connection into the connections map.
     *
     * @param connection The connection to insert.
     * @return A reference to the inserted connection
     */
    Connection&
    insertConnection(ConnectionPtr connection);

    /**
     * @brief Remove a connection from the connections map.
     * @note After this call, the connection reference is no longer valid.
     *
     * @param connection The connection to remove.
     */
    void
    removeConnection(Connection const& connection);

    /**
     * @brief Handle an error.
     *
     * @param error The error to handle.
     * @param connection The connection that caused the error.
     * @return True if the connection should be gracefully closed, false otherwise.
     */
    bool
    handleError(Error const& error, Connection const& connection) const;

    /**
     * @brief The request-response loop.
     *
     * @param connection The connection to handle.
     * @param yield The yield context.
     * @return True if the connection should be gracefully closed, false otherwise.
     */
    bool
    sequentRequestResponseLoop(Connection& connection, boost::asio::yield_context yield);

    bool
    parallelRequestResponseLoop(Connection& connection, boost::asio::yield_context yield);

    std::optional<bool>
    processRequest(Connection& connection, Request&& request, boost::asio::yield_context yield);

    /**
     * @brief Handle a request.
     *
     * @param connectionContext The connection context.
     * @param request The request to handle.
     * @param yield The yield context.
     * @return The response to send.
     */
    Response
    handleRequest(ConnectionContext const& connectionContext, Request const& request, boost::asio::yield_context yield);
};

}  // namespace web::ng::impl
