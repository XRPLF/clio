#pragma once

#include "util/OverloadSet.hpp"
#include "util/Taggable.hpp"
#include "util/build/Build.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/Concepts.hpp"
#include "web/ng/impl/SendingQueue.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace web::ng::impl {

class WsConnectionBase : public Connection {
public:
    using Connection::Connection;

    virtual std::expected<void, Error>
    sendShared(std::shared_ptr<std::string> message, boost::asio::yield_context yield) = 0;
};

template <typename StreamType>
class WsConnection : public WsConnectionBase {
    boost::beast::websocket::stream<StreamType> stream_;
    boost::beast::http::request<boost::beast::http::string_body> initialRequest_;

    using MessageType = std::variant<Response, std::shared_ptr<std::string>>;
    SendingQueue<MessageType> sendingQueue_;

    bool closed_{false};

public:
    WsConnection(
        StreamType&& stream,
        std::string ip,
        boost::beast::flat_buffer buffer,
        boost::beast::http::request<boost::beast::http::string_body> initialRequest,
        util::TagDecoratorFactory const& tagDecoratorFactory
    )
        : WsConnectionBase(std::move(ip), std::move(buffer), tagDecoratorFactory)
        , stream_(std::move(stream))
        , initialRequest_(std::move(initialRequest))
        , sendingQueue_{[this](MessageType const& message, auto&& yield) {
            boost::asio::const_buffer const buffer = std::visit(
                util::OverloadSet{
                    [](Response const& r) -> boost::asio::const_buffer { return r.asWsResponse(); },
                    [](std::shared_ptr<std::string> const& m) -> boost::asio::const_buffer {
                        return boost::asio::buffer(*m);
                    }
                },
                message
            );
            stream_.async_write(buffer, yield);
        }}
    {
        setupWsStream();
    }

    ~WsConnection() override = default;
    WsConnection(WsConnection&&) = delete;
    WsConnection&
    operator=(WsConnection&&) = delete;
    WsConnection(WsConnection const&) = delete;
    WsConnection&
    operator=(WsConnection const&) = delete;

    std::expected<void, Error>
    performHandshake(boost::asio::yield_context yield)
    {
        Error error;
        stream_.async_accept(initialRequest_, yield[error]);
        if (error)
            return std::unexpected{error};
        return {};
    }

    bool
    wasUpgraded() const override
    {
        return true;
    }

    std::expected<void, Error>
    sendShared(std::shared_ptr<std::string> message, boost::asio::yield_context yield) override
    {
        return sendingQueue_.send(std::move(message), yield);
    }

    void
    setTimeout(std::chrono::steady_clock::duration newTimeout) override
    {
        boost::beast::websocket::stream_base::timeout wsTimeout =
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::server
            );
        wsTimeout.idle_timeout = newTimeout;
        wsTimeout.handshake_timeout = newTimeout;
        stream_.set_option(wsTimeout);
    }

    std::expected<void, Error>
    send(Response response, boost::asio::yield_context yield) override
    {
        return sendingQueue_.send(std::move(response), yield);
    }

    std::expected<Request, Error>
    receive(boost::asio::yield_context yield) override
    {
        Error error;
        stream_.async_read(buffer_, yield[error]);
        if (error)
            return std::unexpected{error};

        auto request = boost::beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        return Request{std::move(request), initialRequest_};
    }

    void
    close(boost::asio::yield_context yield) override
    {
        if (closed_)
            return;

        // This should be set before the async_close(). Otherwise there is a possibility to have
        // multiple coroutines waiting on async_close(), but only one will be woken up after the
        // actual close happened, others will hang.
        closed_ = true;

        boost::system::error_code error;  // unused
        stream_.async_close(boost::beast::websocket::close_code::normal, yield[error]);
    }

private:
    void
    setupWsStream()
    {
        // Disable the timeout. The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(stream_).expires_never();
        setTimeout(kDEFAULT_TIMEOUT);
        stream_.set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::response_type& res) {
                    res.set(
                        boost::beast::http::field::server, util::build::getClioFullVersionString()
                    );
                }
            )
        );
    }
};

using PlainWsConnection = WsConnection<boost::beast::tcp_stream>;
using SslWsConnection = WsConnection<boost::asio::ssl::stream<boost::beast::tcp_stream>>;

template <typename StreamType>
std::expected<std::unique_ptr<WsConnection<StreamType>>, Error>
makeWsConnection(
    StreamType&& stream,
    std::string ip,
    boost::beast::flat_buffer buffer,
    boost::beast::http::request<boost::beast::http::string_body> request,
    util::TagDecoratorFactory const& tagDecoratorFactory,
    boost::asio::yield_context yield
)
{
    auto connection = std::make_unique<WsConnection<StreamType>>(
        std::forward<StreamType>(stream),
        std::move(ip),
        std::move(buffer),
        std::move(request),
        tagDecoratorFactory
    );
    auto const expectedSuccess = connection->performHandshake(yield);
    if (not expectedSuccess.has_value())
        return std::unexpected{expectedSuccess.error()};
    return connection;
}

}  // namespace web::ng::impl
