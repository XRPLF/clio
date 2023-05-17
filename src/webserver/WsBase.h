//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <backend/BackendInterface.h>
#include <etl/ETLService.h>
#include <etl/Source.h>
#include <log/Logger.h>
#include <rpc/Counters.h>
#include <rpc/Factories.h>
#include <rpc/RPCEngine.h>
#include <rpc/WorkQueue.h>
#include <subscriptions/Message.h>
#include <subscriptions/SubscriptionManager.h>
#include <util/Profiler.h>
#include <util/Taggable.h>
#include <webserver/DOSGuard.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>

// TODO: Consider removing these. Visible to anyone including this header.
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

inline void
logError(boost::beast::error_code ec, char const* what)
{
    static clio::Logger log{"WebServer"};
    log.debug() << what << ": " << ec.message() << "\n";
}

inline boost::json::object
getDefaultWsResponse(boost::json::value const& id)
{
    boost::json::object defaultResp = {};
    if (!id.is_null())
        defaultResp["id"] = id;

    defaultResp["status"] = "success";
    defaultResp["type"] = "response";

    return defaultResp;
}

class WsBase : public util::Taggable
{
protected:
    clio::Logger log_{"WebServer"};
    clio::Logger perfLog_{"Performance"};
    boost::system::error_code ec_;

public:
    explicit WsBase(util::TagDecoratorFactory const& tagFactory) : Taggable{tagFactory}
    {
    }

    /**
     * @brief Send, that enables SubscriptionManager to publish to clients
     * @param msg The message to send
     */
    virtual void
    send(std::shared_ptr<Message> msg) = 0;

    virtual ~WsBase() = default;

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
};

class SubscriptionManager;
class LoadBalancer;

template <typename Derived>
class WsSession : public WsBase, public std::enable_shared_from_this<WsSession<Derived>>
{
    using std::enable_shared_from_this<WsSession<Derived>>::shared_from_this;

    boost::beast::flat_buffer buffer_;

    boost::asio::io_context& ioc_;
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<RPC::RPCEngine> rpcEngine_;
    // has to be a weak ptr because SubscriptionManager maintains collections
    // of std::shared_ptr<WsBase> objects. If this were shared, there would be
    // a cyclical dependency that would block destruction
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<LoadBalancer> balancer_;
    std::shared_ptr<ETLService const> etl_;
    util::TagDecoratorFactory const& tagFactory_;
    clio::DOSGuard& dosGuard_;
    std::mutex mtx_;

    bool sending_ = false;
    std::queue<std::shared_ptr<Message>> messages_;

protected:
    std::optional<std::string> ip_;

    void
    wsFail(boost::beast::error_code ec, char const* what)
    {
        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            perfLog_.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().ws()).socket().close(ec);

            if (auto manager = subscriptions_.lock(); manager)
                manager->cleanup(derived().shared_from_this());
        }
    }

public:
    explicit WsSession(
        boost::asio::io_context& ioc,
        std::optional<std::string> ip,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<RPC::RPCEngine> rpcEngine,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<LoadBalancer> balancer,
        std::shared_ptr<ETLService const> etl,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        boost::beast::flat_buffer&& buffer)
        : WsBase(tagFactory)
        , buffer_(std::move(buffer))
        , ioc_(ioc)
        , backend_(backend)
        , rpcEngine_(rpcEngine)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , ip_(ip)
    {
        perfLog_.info() << tag() << "session created";
    }

    virtual ~WsSession()
    {
        perfLog_.info() << tag() << "session closed";
        if (ip_)
            dosGuard_.decrement(*ip_);
    }

    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    void
    doWrite()
    {
        sending_ = true;
        derived().ws().async_write(
            net::buffer(messages_.front()->data(), messages_.front()->size()),
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

    void
    send(std::shared_ptr<Message> msg) override
    {
        net::dispatch(
            derived().ws().get_executor(), [this, self = derived().shared_from_this(), msg = std::move(msg)]() {
                messages_.push(std::move(msg));
                maybeSendNext();
            });
    }

    void
    send(std::string&& msg)
    {
        auto sharedMsg = std::make_shared<Message>(std::move(msg));
        send(sharedMsg);
    }

    void
    run(http::request<http::string_body> req)
    {
        // Set suggested timeout settings for the websocket
        derived().ws().set_option(websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
        }));

        derived().ws().async_accept(
            req, boost::beast::bind_front_handler(&WsSession::onAccept, this->shared_from_this()));
    }

    void
    onAccept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        perfLog_.info() << tag() << "accepting new connection";

        // Read a message
        doRead();
    }

    void
    doRead()
    {
        if (dead())
            return;

        std::lock_guard<std::mutex> lck{mtx_};
        // Clear the buffer
        buffer_.consume(buffer_.size());
        // Read a message into our buffer
        derived().ws().async_read(
            buffer_, boost::beast::bind_front_handler(&WsSession::onRead, this->shared_from_this()));
    }

    void
    handleRequest(boost::json::object const&& request, boost::json::value const& id, boost::asio::yield_context& yield)
    {
        auto ip = derived().ip();
        if (!ip)
            return;

        boost::json::object response = {};
        auto sendError = [this, &request, id](auto error) {
            auto e = RPC::makeError(error);
            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;
            this->send(boost::json::serialize(e));
        };

        try
        {
            log_.info() << tag() << "ws received request from work queue : " << request;

            auto range = backend_->fetchLedgerRange();
            if (!range)
                return sendError(RPC::RippledError::rpcNOT_READY);

            auto context = RPC::make_WsContext(
                yield, request, shared_from_this(), tagFactory_.with(std::cref(tag())), *range, *ip);

            if (!context)
            {
                perfLog_.warn() << tag() << "Could not create RPC context";
                return sendError(RPC::RippledError::rpcBAD_SYNTAX);
            }

            response = getDefaultWsResponse(id);

            auto [v, timeDiff] = util::timed([this, &context]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            RPC::logDuration(*context, us);

            if (auto status = std::get_if<RPC::Status>(&v))
            {
                rpcEngine_->notifyErrored(context->method);
                auto error = RPC::makeError(*status);

                if (!id.is_null())
                    error["id"] = id;

                error["request"] = request;
                response = error;
            }
            else
            {
                rpcEngine_->notifyComplete(context->method, us);

                auto const& result = std::get<boost::json::object>(v);

                auto const isForwarded = result.contains("forwarded") && result.at("forwarded").is_bool() &&
                    result.at("forwarded").as_bool();

                // if the result is forwarded - just use it as is
                // but keep all default fields in the response too.
                if (isForwarded)
                    for (auto const& [k, v] : result)
                        response.insert_or_assign(k, v);
                else
                    response["result"] = result;
            }
        }
        catch (std::exception const& e)
        {
            perfLog_.error() << tag() << "Caught exception : " << e.what();

            return sendError(RPC::RippledError::rpcINTERNAL);
        }

        boost::json::array warnings;

        warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_CLIO));

        auto lastCloseAge = etl_->lastCloseAgeSeconds();
        if (lastCloseAge >= 60)
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_OUTDATED));
        response["warnings"] = warnings;
        std::string responseStr = boost::json::serialize(response);
        if (!dosGuard_.add(*ip, responseStr.size()))
        {
            response["warning"] = "load";
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_RATE_LIMIT));
            response["warnings"] = warnings;
            // reserialize if we need to include this warning
            responseStr = boost::json::serialize(response);
        }
        send(std::move(responseStr));
    }

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "read");

        std::string msg{static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        auto ip = derived().ip();

        if (!ip)
            return;

        perfLog_.info() << tag() << "Received request from ip = " << *ip;

        auto sendError = [this, ip](auto error, boost::json::value const& id, boost::json::object const& request) {
            auto e = RPC::makeError(error);

            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;

            auto responseStr = boost::json::serialize(e);
            log_.trace() << responseStr;
            dosGuard_.add(*ip, responseStr.size());
            send(std::move(responseStr));
        };

        boost::json::value raw = [](std::string const&& msg) {
            try
            {
                return boost::json::parse(msg);
            }
            catch (std::exception&)
            {
                return boost::json::value{nullptr};
            }
        }(std::move(msg));

        boost::json::object request;
        // dosGuard served request++ and check ip address
        // dosGuard should check before any request, even invalid request
        if (!dosGuard_.request(*ip))
        {
            sendError(RPC::RippledError::rpcSLOW_DOWN, nullptr, request);
        }
        else if (!raw.is_object())
        {
            // handle invalid request and async read again
            sendError(RPC::RippledError::rpcINVALID_PARAMS, nullptr, request);
        }
        else
        {
            request = raw.as_object();

            auto id = request.contains("id") ? request.at("id") : nullptr;
            perfLog_.debug() << tag() << "Adding to work queue";

            if (not rpcEngine_->post(
                    [self = shared_from_this(), req = std::move(request), id](boost::asio::yield_context yield) {
                        self->handleRequest(std::move(req), id, yield);
                    },
                    ip.value()))
                sendError(RPC::RippledError::rpcTOO_BUSY, id, request);
        }

        doRead();
    }
};
