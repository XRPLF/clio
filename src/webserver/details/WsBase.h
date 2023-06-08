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

#include <log/Logger.h>
#include <rpc/common/Types.h>
#include <webserver/DOSGuard.h>
#include <webserver/interface/Concepts.h>
#include <webserver/interface/ConnectionBase.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>

namespace Server {

/**
 * @brief Web socket implementation. This class is the base class of the web socket session, it will handle the read and
 * write operations.
 * The write operation is via a queue, each write operation of this session will be sent in order.
 * The write operation also supports shared_ptr of string, so the caller can keep the string alive until it is sent. It
 * is useful when we have multiple sessions sending the same content
 * @tparam Derived The derived class
 * @tparam Handler The handler type, will be called when a request is received.
 */
template <template <class> class Derived, ServerHandler Handler>
class WsSession : public ConnectionBase, public std::enable_shared_from_this<WsSession<Derived, Handler>>
{
    using std::enable_shared_from_this<WsSession<Derived, Handler>>::shared_from_this;

    boost::beast::flat_buffer buffer_;
    std::reference_wrapper<clio::DOSGuard> dosGuard_;
    bool sending_ = false;
    std::queue<std::shared_ptr<std::string>> messages_;
    std::shared_ptr<Handler> const handler_;

protected:
    clio::Logger log_{"WebServer"};
    clio::Logger perfLog_{"Performance"};

    void
    wsFail(boost::beast::error_code ec, char const* what)
    {
        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            perfLog_.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().ws()).socket().close(ec);
            (*handler_)(ec, derived().shared_from_this());
        }
    }

public:
    explicit WsSession(
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer&& buffer)
        : ConnectionBase(tagFactory, ip), buffer_(std::move(buffer)), dosGuard_(dosGuard), handler_(handler)
    {
        upgraded = true;
        perfLog_.debug() << tag() << "session created";
    }

    virtual ~WsSession()
    {
        perfLog_.debug() << tag() << "session closed";
        dosGuard_.get().decrement(clientIp);
    }

    Derived<Handler>&
    derived()
    {
        return static_cast<Derived<Handler>&>(*this);
    }

    void
    doWrite()
    {
        sending_ = true;
        derived().ws().async_write(
            boost::asio::buffer(messages_.front()->data(), messages_.front()->size()),
            boost::beast::bind_front_handler(&WsSession::onWrite, derived().shared_from_this()));
    }

    void
    onWrite(boost::system::error_code ec, std::size_t)
    {
        if (ec)
        {
            wsFail(ec, "Failed to write");
        }
        else
        {
            messages_.pop();
            sending_ = false;
            maybeSendNext();
        }
    }

    void
    maybeSendNext()
    {
        if (ec_ || sending_ || messages_.empty())
            return;

        doWrite();
    }

    /**
     * @brief Send a message to the client
     * @param msg The message to send, it will keep the string alive until it is sent. It is useful when we have
     * multiple session sending the same content.
     * Be aware that the message length will not be added to the DOSGuard from this function.
     */
    void
    send(std::shared_ptr<std::string> msg) override
    {
        boost::asio::dispatch(
            derived().ws().get_executor(), [this, self = derived().shared_from_this(), msg = std::move(msg)]() {
                messages_.push(std::move(msg));
                maybeSendNext();
            });
    }

    /**
     * @brief Send a message to the client
     * @param msg The message to send
     * Send this message to the client. The message length will be added to the DOSGuard
     * If the DOSGuard is triggered, the message will be modified to include a warning
     */
    void
    send(std::string&& msg, http::status _ = http::status::ok) override
    {
        if (!dosGuard_.get().add(clientIp, msg.size()))
        {
            auto jsonResponse = boost::json::parse(msg).as_object();
            jsonResponse["warning"] = "load";

            if (jsonResponse.contains("warnings") && jsonResponse["warnings"].is_array())
                jsonResponse["warnings"].as_array().push_back(RPC::makeWarning(RPC::warnRPC_RATE_LIMIT));
            else
                jsonResponse["warnings"] = boost::json::array{RPC::makeWarning(RPC::warnRPC_RATE_LIMIT)};

            // reserialize when we need to include this warning
            msg = boost::json::serialize(jsonResponse);
        }
        auto sharedMsg = std::make_shared<std::string>(std::move(msg));
        send(std::move(sharedMsg));
    }

    /**
     * @brief Accept the session asynchroniously
     */
    void
    run(http::request<http::string_body> req)
    {
        using namespace boost::beast;

        derived().ws().set_option(websocket::stream_base::timeout::suggested(role_type::server));

        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
        }));

        derived().ws().async_accept(req, bind_front_handler(&WsSession::onAccept, this->shared_from_this()));
    }

    void
    onAccept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        perfLog_.info() << tag() << "accepting new connection";

        doRead();
    }

    void
    doRead()
    {
        if (dead())
            return;

        // Clear the buffer
        buffer_.consume(buffer_.size());

        derived().ws().async_read(
            buffer_, boost::beast::bind_front_handler(&WsSession::onRead, this->shared_from_this()));
    }

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "read");

        perfLog_.info() << tag() << "Received request from ip = " << this->clientIp;

        auto sendError = [this](auto error, std::string const& requestStr) {
            auto e = RPC::makeError(error);
            try
            {
                auto request = boost::json::parse(requestStr);
                if (request.is_object() && request.as_object().contains("id"))
                    e["id"] = request.as_object().at("id");
                e["request"] = std::move(request);
            }
            catch (std::exception&)
            {
                e["request"] = std::move(requestStr);
            }

            auto responseStr = boost::json::serialize(e);
            log_.trace() << responseStr;
            auto sharedMsg = std::make_shared<std::string>(std::move(responseStr));
            send(std::move(sharedMsg));
        };

        std::string msg{static_cast<char const*>(buffer_.data().data()), buffer_.size()};

        // dosGuard served request++ and check ip address
        if (!dosGuard_.get().request(clientIp))
        {
            sendError(RPC::RippledError::rpcSLOW_DOWN, std::move(msg));
        }
        else
        {
            try
            {
                (*handler_)(std::move(msg), shared_from_this());
            }
            catch (std::exception const& e)
            {
                perfLog_.error() << tag() << "Caught exception : " << e.what();
                sendError(RPC::RippledError::rpcINTERNAL, std::move(msg));
            }
        }

        doRead();
    }
};
}  // namespace Server
