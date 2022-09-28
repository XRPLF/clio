#ifndef RIPPLE_REPORTING_WS_BASE_SESSION_H
#define RIPPLE_REPORTING_WS_BASE_SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>

#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <etl/ReportingETL.h>
#include <rpc/Counters.h>
#include <rpc/RPC.h>
#include <rpc/WorkQueue.h>
#include <subscriptions/Message.h>
#include <subscriptions/SubscriptionManager.h>
#include <util/Taggable.h>
#include <webserver/DOSGuard.h>

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

class WsBase : public util::Taggable
{
protected:
    boost::system::error_code ec_;

public:
    explicit WsBase(util::TagDecoratorFactory const& tagFactory)
        : Taggable{tagFactory}
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
class ETLLoadBalancer;

// Echoes back all received WebSocket messages
template <typename Derived>
class WsSession : public WsBase,
                  public std::enable_shared_from_this<WsSession<Derived>>
{
    using std::enable_shared_from_this<WsSession<Derived>>::shared_from_this;

    boost::beast::flat_buffer buffer_;

    boost::asio::io_context& ioc_;
    std::shared_ptr<BackendInterface const> backend_;
    // has to be a weak ptr because SubscriptionManager maintains collections
    // of std::shared_ptr<WsBase> objects. If this were shared, there would be
    // a cyclical dependency that would block destruction
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    std::shared_ptr<ReportingETL const> etl_;
    util::TagDecoratorFactory const& tagFactory_;
    DOSGuard& dosGuard_;
    RPC::Counters& counters_;
    WorkQueue& queue_;
    std::mutex mtx_;

    bool sending_ = false;
    std::queue<std::shared_ptr<Message>> messages_;

    void
    wsFail(boost::beast::error_code ec, char const* what)
    {
        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            BOOST_LOG_TRIVIAL(info)
                << tag() << __func__ << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().ws()).socket().close(ec);

            if (auto manager = subscriptions_.lock(); manager)
                manager->cleanup(derived().shared_from_this());
        }
    }

public:
    explicit WsSession(
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer&& buffer)
        : WsBase(tagFactory)
        , buffer_(std::move(buffer))
        , ioc_(ioc)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , counters_(counters)
        , queue_(queue)
    {
        BOOST_LOG_TRIVIAL(info) << tag() << "session created";
    }
    virtual ~WsSession()
    {
        BOOST_LOG_TRIVIAL(info) << tag() << "session closed";
    }

    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    void
    do_write()
    {
        sending_ = true;
        derived().ws().async_write(
            net::buffer(messages_.front()->data(), messages_.front()->size()),
            boost::beast::bind_front_handler(
                &WsSession::on_write, derived().shared_from_this()));
    }

    void
    on_write(boost::system::error_code ec, std::size_t)
    {
        if (ec)
        {
            wsFail(ec, "Failed to write");
        }
        else
        {
            messages_.pop();
            sending_ = false;
            maybe_send_next();
        }
    }

    void
    maybe_send_next()
    {
        if (ec_ || sending_ || messages_.empty())
            return;

        do_write();
    }

    void
    send(std::shared_ptr<Message> msg) override
    {
        net::dispatch(
            derived().ws().get_executor(),
            [this,
             self = derived().shared_from_this(),
             msg = std::move(msg)]() {
                messages_.push(std::move(msg));
                maybe_send_next();
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

        BOOST_LOG_TRIVIAL(info) << tag() << "accepting new connection";

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        if (dead())
            return;

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
        boost::json::object const&& request,
        boost::json::value const& id,
        boost::asio::yield_context& yield)
    {
        auto ip = derived().ip();
        if (!ip)
            return;

        boost::json::object response = {};
        auto sendError = [this, &request, id](auto error) {
            auto e = RPC::make_error(error);
            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;
            this->send(boost::json::serialize(e));
        };

        try
        {
            BOOST_LOG_TRIVIAL(debug)
                << tag() << "ws received request from work queue : " << request;

            auto range = backend_->fetchLedgerRange();
            if (!range)
                return sendError(RPC::Error::rpcNOT_READY);

            std::optional<RPC::Context> context = RPC::make_WsContext(
                yield,
                request,
                backend_,
                subscriptions_.lock(),
                balancer_,
                etl_,
                shared_from_this(),
                tagFactory_.with(std::cref(tag())),
                *range,
                counters_,
                *ip);

            if (!context)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << tag() << " could not create RPC context";
                return sendError(RPC::Error::rpcBAD_SYNTAX);
            }

            response = getDefaultWsResponse(id);

            auto start = std::chrono::system_clock::now();
            auto v = RPC::buildResponse(*context);
            auto end = std::chrono::system_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start);
            logDuration(*context, us);

            if (auto status = std::get_if<RPC::Status>(&v))
            {
                counters_.rpcErrored(context->method);

                auto error = RPC::make_error(*status);

                if (!id.is_null())
                    error["id"] = id;

                error["request"] = request;
                response = error;
            }
            else
            {
                counters_.rpcComplete(context->method, us);

                response["result"] = std::get<boost::json::object>(v);
            }
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(error)
                << tag() << __func__ << " caught exception : " << e.what();

            return sendError(RPC::Error::rpcINTERNAL);
        }

        boost::json::array warnings;

        warnings.emplace_back(RPC::make_warning(RPC::warnRPC_CLIO));

        auto lastCloseAge = etl_->lastCloseAgeSeconds();
        if (lastCloseAge >= 60)
            warnings.emplace_back(RPC::make_warning(RPC::warnRPC_OUTDATED));
        response["warnings"] = warnings;
        std::string responseStr = boost::json::serialize(response);
        if (!dosGuard_.add(*ip, responseStr.size()))
        {
            response["warning"] = "load";
            warnings.emplace_back(RPC::make_warning(RPC::warnRPC_RATE_LIMIT));
            response["warnings"] = warnings;
            // reserialize if we need to include this warning
            responseStr = boost::json::serialize(response);
        }
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

        if (!ip)
            return;

        BOOST_LOG_TRIVIAL(info) << tag() << "ws::" << __func__
                                << " received request from ip = " << *ip;

        auto sendError = [this, ip](
                             auto error,
                             boost::json::value const& id,
                             boost::json::object const& request) {
            auto e = RPC::make_error(error);

            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;

            auto responseStr = boost::json::serialize(e);
            BOOST_LOG_TRIVIAL(trace) << __func__ << " : " << responseStr;
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
        if (!raw.is_object())
            return sendError(RPC::Error::rpcINVALID_PARAMS, nullptr, request);
        request = raw.as_object();

        auto id = request.contains("id") ? request.at("id") : nullptr;

        if (!dosGuard_.isOk(*ip))
        {
            sendError(RPC::Error::rpcSLOW_DOWN, id, request);
        }
        else
        {
            BOOST_LOG_TRIVIAL(debug)
                << tag() << __func__ << " adding to work queue";

            if (!queue_.postCoro(
                    [shared_this = shared_from_this(),
                     r = std::move(request),
                     id](boost::asio::yield_context yield) {
                        shared_this->handle_request(std::move(r), id, yield);
                    },
                    dosGuard_.isWhiteListed(*ip)))
                sendError(RPC::Error::rpcTOO_BUSY, id, request);
        }

        do_read();
    }
};

#endif  // RIPPLE_REPORTING_WS_BASE_SESSION_H
