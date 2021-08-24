//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2021 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_REPORTING_WS_BASE_SESSION_H
#define RIPPLE_REPORTING_WS_BASE_SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>

#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <rpc/RPC.h>
#include <webserver/DOSGuard.h>
#include <webserver/SubscriptionManager.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

inline void
logError(boost::beast::error_code ec, char const* what)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : " << what << ": " << ec.message() << "\n";
}

inline boost::json::object
getDefaultWsResponse(boost::json::value const& id)
{
    boost::json::object defaultResp = {};
    if (!id.is_null())
        defaultResp["id"] = id;

    defaultResp["result"] = boost::json::object_kind;
    defaultResp["status"] = "success";
    defaultResp["type"] = "response";

    return defaultResp;
}

class WsBase
{
    std::atomic_bool dead_ = false;

public:
    // Send, that enables SubscriptionManager to publish to clients
    virtual void
    publishToStream(std::string const& msg) = 0;

    virtual ~WsBase()
    {
    }

    void
    wsFail(boost::beast::error_code ec, char const* what)
    {
        logError(ec, what);
        dead_ = true;
    }

    bool
    dead()
    {
        return dead_;
    }
};

class SubscriptionManager;
class ETLLoadBalancer;

// Echoes back all received WebSocket messages
template <class Derived>
class WsSession : public WsBase,
                  public std::enable_shared_from_this<WsSession<Derived>>
{
    using std::enable_shared_from_this<WsSession<Derived>>::shared_from_this;

    boost::beast::flat_buffer buffer_;
    std::string responseBuffer_;

    std::shared_ptr<BackendInterface> backend_;
    // has to be a weak ptr because SubscriptionManager maintains collections
    // of std::shared_ptr<WsBase> objects. If this were shared, there would be
    // a cyclical dependency that would block destruction
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    std::mutex mtx_;

public:
    explicit WsSession(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer&& buffer)
        : backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , buffer_(std::move(buffer))
    {
    }
    virtual ~WsSession()
    {
    }

    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    void
    publishToStream(std::string const& msg)
    {
        // msg might not stick around for as long as it takes to write, so we
        // make a copy and capture it in the lambda
        auto msgSaved = std::make_unique<std::string>(msg);

        std::lock_guard<std::mutex> lck(mtx_);
        derived().ws().text(derived().ws().got_text());
        // we send from SubscriptionManager as well as from on_read
        derived().ws().async_write(
            boost::asio::buffer(*msgSaved),
            [m = std::move(msgSaved), shared = shared_from_this()](
                auto ec, size_t size) {
                if (ec)
                    return shared->wsFail(ec, "publishToStream");
            });
    }
    void
    sendResponse(boost::json::object const& object)
    {
        std::lock_guard<std::mutex> lck(mtx_);
        responseBuffer_ = boost::json::serialize(object);
        derived().ws().text(derived().ws().got_text());
        derived().ws().async_write(
            boost::asio::buffer(responseBuffer_),
            boost::beast::bind_front_handler(
                &WsSession::on_write, this->shared_from_this()));
    }

    void
    run(http::request<http::string_body> req)
    {
        // Set suggested timeout settings for the websocket
        derived().ws().set_option(websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(
                    http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));

        derived().ws().async_accept(
            req,
            boost::beast::bind_front_handler(
                &WsSession::on_accept, this->shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        std::lock_guard<std::mutex> lck{mtx_};
        // Read a message into our buffer
        derived().ws().async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &WsSession::on_read, this->shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "read");

        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        boost::json::object response;
        auto ip = derived().ip();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " received request from ip = " << ip;
        if (!dosGuard_.isOk(ip))
            response["error"] = "Too many requests. Slow down";
        else
        {
            try
            {
                boost::json::value raw = boost::json::parse(msg);
                boost::json::object request = raw.as_object();
                BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
                try
                {
                    auto range = backend_->fetchLedgerRange();
                    if (!range)
                        return sendResponse(
                            RPC::make_error(RPC::Error::rpcNOT_READY));

                    std::optional<RPC::Context> context = RPC::make_WsContext(
                        request,
                        backend_,
                        subscriptions_.lock(),
                        balancer_,
                        shared_from_this(),
                        *range);

                    if (!context)
                        return sendResponse(
                            RPC::make_error(RPC::Error::rpcBAD_SYNTAX));

                    auto id =
                        request.contains("id") ? request.at("id") : nullptr;

                    response = getDefaultWsResponse(id);
                    boost::json::object& result =
                        response["result"].as_object();

                    auto v = RPC::buildResponse(*context);

                    if (auto status = std::get_if<RPC::Status>(&v))
                    {
                        auto error =
                            RPC::make_error(status->error, status->message);

                        if (!id.is_null())
                            error["id"] = id;
                        error["request"] = request;
                        response = error;
                    }
                    else
                    {
                        response = std::get<boost::json::object>(v);
                    }
                }
                catch (Backend::DatabaseTimeout const& t)
                {
                    BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                    return sendResponse(
                        RPC::make_error(RPC::Error::rpcNOT_READY));
                }
            }
            catch (std::exception const& e)
            {
                BOOST_LOG_TRIVIAL(error)
                    << __func__ << " caught exception : " << e.what();

                return sendResponse(RPC::make_error(RPC::Error::rpcINTERNAL));
            }
        }
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " : " << boost::json::serialize(response);

        sendResponse(response);
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());
        responseBuffer_.clear();
        // Do another read
        do_read();
    }
};

#endif  // RIPPLE_REPORTING_WS_BASE_SESSION_H
