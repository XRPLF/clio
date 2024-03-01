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

#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "web/DOSGuard.hpp"
#include "web/interface/Concepts.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/json/array.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <ripple/protocol/ErrorCodes.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>

namespace web::impl {

/**
 * @brief Web socket implementation. This class is the base class of the web socket session, it will handle the read and
 * write operations.
 * The write operation is via a queue, each write operation of this session will be sent in order.
 * The write operation also supports shared_ptr of string, so the caller can keep the string alive until it is sent. It
 * is useful when we have multiple sessions sending the same content
 * @tparam Derived The derived class
 * @tparam HandlerType The handler type, will be called when a request is received.
 */
template <template <typename> class Derived, SomeServerHandler HandlerType>
class WsBase : public ConnectionBase, public std::enable_shared_from_this<WsBase<Derived, HandlerType>> {
    using std::enable_shared_from_this<WsBase<Derived, HandlerType>>::shared_from_this;

    boost::beast::flat_buffer buffer_;
    std::reference_wrapper<web::DOSGuard> dosGuard_;
    bool sending_ = false;
    std::queue<std::shared_ptr<std::string>> messages_;
    std::shared_ptr<HandlerType> const handler_;

protected:
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};

    void
    wsFail(boost::beast::error_code ec, char const* what)
    {
        LOG(perfLog_.error()) << tag() << ": " << what << ": " << ec.message();
        if (!ec_ && ec != boost::asio::error::operation_aborted) {
            ec_ = ec;
            boost::beast::get_lowest_layer(derived().ws()).socket().close(ec);
        }
    }

public:
    explicit WsBase(
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<HandlerType> const& handler,
        boost::beast::flat_buffer&& buffer
    )
        : ConnectionBase(tagFactory, ip), buffer_(std::move(buffer)), dosGuard_(dosGuard), handler_(handler)
    {
        upgraded = true;  // NOLINT (cppcoreguidelines-pro-type-member-init)
        LOG(perfLog_.debug()) << tag() << "session created";
    }

    ~WsBase() override
    {
        LOG(perfLog_.debug()) << tag() << "session closed";
        dosGuard_.get().decrement(clientIp);
    }

    Derived<HandlerType>&
    derived()
    {
        return static_cast<Derived<HandlerType>&>(*this);
    }

    void
    doWrite()
    {
        sending_ = true;
        derived().ws().async_write(
            boost::asio::buffer(messages_.front()->data(), messages_.front()->size()),
            boost::beast::bind_front_handler(&WsBase::onWrite, derived().shared_from_this())
        );
    }

    void
    onWrite(boost::system::error_code ec, std::size_t)
    {
        messages_.pop();
        sending_ = false;
        if (ec) {
            wsFail(ec, "Failed to write");
        } else {
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
            derived().ws().get_executor(),
            [this, self = derived().shared_from_this(), msg = std::move(msg)]() {
                messages_.push(msg);
                maybeSendNext();
            }
        );
    }

    /**
     * @brief Send a message to the client
     * @param msg The message to send
     * Send this message to the client. The message length will be added to the DOSGuard
     * If the DOSGuard is triggered, the message will be modified to include a warning
     */
    void
    send(std::string&& msg, http::status) override
    {
        if (!dosGuard_.get().add(clientIp, msg.size())) {
            auto jsonResponse = boost::json::parse(msg).as_object();
            jsonResponse["warning"] = "load";

            if (jsonResponse.contains("warnings") && jsonResponse["warnings"].is_array()) {
                jsonResponse["warnings"].as_array().push_back(rpc::makeWarning(rpc::warnRPC_RATE_LIMIT));
            } else {
                jsonResponse["warnings"] = boost::json::array{rpc::makeWarning(rpc::warnRPC_RATE_LIMIT)};
            }

            // Reserialize when we need to include this warning
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

        derived().ws().async_accept(req, bind_front_handler(&WsBase::onAccept, this->shared_from_this()));
    }

    void
    onAccept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        LOG(perfLog_.info()) << tag() << "accepting new connection";

        doRead();
    }

    void
    doRead()
    {
        if (dead())
            return;

        // Note: use entirely new buffer so previously used, potentially large, capacity is deallocated
        buffer_ = boost::beast::flat_buffer{};

        derived().ws().async_read(buffer_, boost::beast::bind_front_handler(&WsBase::onRead, this->shared_from_this()));
    }

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "read");

        LOG(perfLog_.info()) << tag() << "Received request from ip = " << this->clientIp;

        auto sendError = [this](auto error, std::string&& requestStr) {
            auto e = rpc::makeError(error);

            try {
                auto request = boost::json::parse(requestStr);
                if (request.is_object() && request.as_object().contains("id"))
                    e["id"] = request.as_object().at("id");
                e["request"] = std::move(request);
            } catch (std::exception const&) {
                e["request"] = std::move(requestStr);
            }

            this->send(std::make_shared<std::string>(boost::json::serialize(e)));
        };

        std::string requestStr{static_cast<char const*>(buffer_.data().data()), buffer_.size()};

        // dosGuard served request++ and check ip address
        if (!dosGuard_.get().request(clientIp)) {
            // TODO: could be useful to count in counters in the future too
            sendError(rpc::RippledError::rpcSLOW_DOWN, std::move(requestStr));
        } else {
            try {
                (*handler_)(requestStr, shared_from_this());
            } catch (std::exception const&) {
                sendError(rpc::RippledError::rpcINTERNAL, std::move(requestStr));
            }
        }

        doRead();
    }
};
}  // namespace web::impl
