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
#include <rpc/Counters.h>
#include <subscriptions/SubscriptionManager.h>

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
    send(std::string const& msg) = 0;

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

    std::shared_ptr<BackendInterface const> backend_;
    // has to be a weak ptr because SubscriptionManager maintains collections
    // of std::shared_ptr<WsBase> objects. If this were shared, there would be
    // a cyclical dependency that would block destruction
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    RPC::Counters& counters_;
    std::mutex mtx_;
    std::queue<std::string> messages_;

public:
    explicit WsSession(
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        boost::beast::flat_buffer&& buffer)
        : backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , counters_(counters)
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
    sendNext()
    {
        std::lock_guard<std::mutex> lck(mtx_);
        derived().ws().async_write(
            boost::asio::buffer(messages_.front()),
            [shared = shared_from_this()](auto ec, size_t size) {
                if (ec)
                    return shared->wsFail(ec, "publishToStream");
                size_t left = 0;
                {
                    std::lock_guard<std::mutex> lck(shared->mtx_);
                    shared->messages_.pop();
                    left = shared->messages_.size();
                }
                if (left > 0)
                    shared->sendNext();
            });
    }

    void
    enqueueMessage(std::string&& msg)
    {
        size_t left = 0;
        {
            std::lock_guard<std::mutex> lck(mtx_);
            messages_.push(std::move(msg));
            left = messages_.size();
        }
        // if the queue was previously empty, start the send chain
        if (left == 1)
            sendNext();
    }

    void
    send(std::string const& msg) override
    {
        enqueueMessage(std::string(msg));
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
        // Clear the buffer
        buffer_.consume(buffer_.size());
        // Read a message into our buffer
        derived().ws().async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &WsSession::on_read, this->shared_from_this()));
    }

    void
    handle_request(
        std::string const&& msg, 
        boost::asio::yield_context& yc)
    {
        boost::json::object response = {};
        auto sendError = [this](auto error) {
            send(boost::json::serialize(RPC::make_error(error)));
        };
        try
        {
            boost::json::value raw = boost::json::parse(msg);
            boost::json::object request = raw.as_object();
            BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
            try
            {
                auto range = backend_->fetchLedgerRange();
                if (!range)
                    return sendError(RPC::Error::rpcNOT_READY);

                std::optional<RPC::Context> context = RPC::make_WsContext(
                    yc,
                    request,
                    backend_,
                    subscriptions_.lock(),
                    balancer_,
                    shared_from_this(),
                    *range,
                    counters_,
                    derived().ip());


                if (!context)
                    return sendError(RPC::Error::rpcBAD_SYNTAX);

                auto id =
                    request.contains("id") ? request.at("id") : nullptr;

                response = getDefaultWsResponse(id);
                boost::json::object& result =
                    response["result"].as_object();
                
                auto v = RPC::buildResponse(*context);

                if (auto status = std::get_if<RPC::Status>(&v))
                {
                    auto error = RPC::make_error(*status);

                    if (!id.is_null())
                        error["id"] = id;
                    error["request"] = request;
                    response = error;
                }
                else
                {
                    result = std::get<boost::json::object>(v);
                }
            }
            catch (Backend::DatabaseTimeout const& t)
            {
                BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                return sendError(RPC::Error::rpcNOT_READY);
            }
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__ << " caught exception : " << e.what();

            return sendError(RPC::Error::rpcINTERNAL);
        }

        std::string responseStr = boost::json::serialize(response);
        dosGuard_.add(derived().ip(), responseStr.size());
        send(std::move(responseStr));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "read");

        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        auto ip = derived().ip();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " received request from ip = " << ip;
        if (!dosGuard_.isOk(ip))
        {
            boost::json::object response;
            response["error"] = "Too many requests. Slow down";
            std::string responseStr = boost::json::serialize(response);

            BOOST_LOG_TRIVIAL(trace)
                << __func__ << " : " << responseStr;

            dosGuard_.add(ip, responseStr.size());
            send(std::move(responseStr));
        }
        else
        {
            boost::asio::spawn(
                derived().ws().get_executor(),
                [m = std::move(msg), this] (boost::asio::yield_context yc)
                {
                    handle_request(std::move(m), yc);
                });
        }


        do_read();
    }
};

#endif  // RIPPLE_REPORTING_WS_BASE_SESSION_H
