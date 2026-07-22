#pragma once

#include "util/Assert.hpp"
#include "util/Taggable.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/Concepts.hpp"
#include "web/ng/impl/SendingQueue.hpp"
#include "web/ng/impl/WsConnection.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/basic_stream.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng::impl {

class UpgradableConnection : public Connection {
public:
    using Connection::Connection;

    virtual std::expected<bool, Error>
    isUpgradeRequested(boost::asio::yield_context yield) = 0;

    virtual std::expected<ConnectionPtr, Error>
    upgrade(
        util::TagDecoratorFactory const& tagDecoratorFactory,
        boost::asio::yield_context yield
    ) = 0;

    virtual std::expected<void, Error>
    sendRaw(
        boost::beast::http::response<boost::beast::http::string_body> response,
        boost::asio::yield_context yield
    ) = 0;
};

using UpgradableConnectionPtr = std::unique_ptr<UpgradableConnection>;

template <typename StreamType>
class HttpConnection : public UpgradableConnection {
    StreamType stream_;
    std::optional<boost::beast::http::request<boost::beast::http::string_body>> request_;
    std::chrono::steady_clock::duration timeout_{kDefaultTimeout};

    using MessageType = boost::beast::http::response<boost::beast::http::string_body>;
    SendingQueue<MessageType> sendingQueue_;

    bool closed_{false};

public:
    HttpConnection(
        boost::asio::ip::tcp::socket socket,
        std::string ip,
        boost::beast::flat_buffer buffer,
        util::TagDecoratorFactory const& tagDecoratorFactory
    )
        requires IsTcpStream<StreamType>
        : UpgradableConnection(std::move(ip), std::move(buffer), tagDecoratorFactory)
        , stream_{std::move(socket)}
        , sendingQueue_([this](MessageType const& message, auto&& yield) {
            boost::beast::get_lowest_layer(stream_).expires_after(timeout_);
            boost::beast::http::async_write(stream_, message, yield);
        })
    {
    }

    HttpConnection(
        boost::asio::ip::tcp::socket socket,
        std::string ip,
        boost::beast::flat_buffer buffer,
        boost::asio::ssl::context& sslCtx,
        util::TagDecoratorFactory const& tagDecoratorFactory
    )
        requires IsSslTcpStream<StreamType>
        : UpgradableConnection(std::move(ip), std::move(buffer), tagDecoratorFactory)
        , stream_{std::move(socket), sslCtx}
        , sendingQueue_([this](MessageType const& message, auto&& yield) {
            boost::beast::get_lowest_layer(stream_).expires_after(timeout_);
            boost::beast::http::async_write(stream_, message, yield);
        })
    {
    }

    HttpConnection(HttpConnection&& other) = delete;
    HttpConnection&
    operator=(HttpConnection&& other) = delete;
    HttpConnection(HttpConnection const& other) = delete;
    HttpConnection&
    operator=(HttpConnection const& other) = delete;

    std::expected<void, Error>
    sslHandshake(boost::asio::yield_context yield)
        requires IsSslTcpStream<StreamType>
    {
        boost::system::error_code error;
        boost::beast::get_lowest_layer(stream_).expires_after(timeout_);
        auto const bytesUsed = stream_.async_handshake(
            boost::asio::ssl::stream_base::server, buffer_.cdata(), yield[error]
        );
        if (error)
            return std::unexpected{error};

        buffer_.consume(bytesUsed);

        return {};
    }

    [[nodiscard]] bool
    wasUpgraded() const override
    {
        return false;
    }

    std::expected<void, Error>
    sendRaw(
        boost::beast::http::response<boost::beast::http::string_body> response,
        boost::asio::yield_context yield
    ) override
    {
        return sendingQueue_.send(std::move(response), yield);
    }

    void
    setTimeout(std::chrono::steady_clock::duration newTimeout) override
    {
        timeout_ = newTimeout;
    }

    std::expected<void, Error>
    send(Response response, boost::asio::yield_context yield) override
    {
        auto httpResponse = std::move(response).intoHttpResponse();
        return sendRaw(std::move(httpResponse), yield);
    }

    std::expected<Request, Error>
    receive(boost::asio::yield_context yield) override
    {
        if (request_.has_value()) {
            Request result{*std::move(request_)};
            request_.reset();
            return result;
        }
        auto expectedRequest = fetch(yield);
        if (expectedRequest.has_value())
            return Request{std::move(expectedRequest).value()};

        return std::unexpected{std::move(expectedRequest).error()};
    }

    void
    close(boost::asio::yield_context yield) override
    {
        // This is needed because calling async_shutdown() multiple times may lead to hanging
        // coroutines. See WsConnection for more details.
        if (closed_)
            return;

        closed_ = true;

        [[maybe_unused]] boost::system::error_code error;
        if constexpr (IsSslTcpStream<StreamType>) {
            boost::beast::get_lowest_layer(stream_).expires_after(timeout_);
            stream_.async_shutdown(yield[error]);  // Close the SSL connection gracefully
        }
        boost::beast::get_lowest_layer(stream_).socket().shutdown(
            boost::asio::ip::tcp::socket::shutdown_type::shutdown_both, error
        );
    }

    std::expected<bool, Error>
    isUpgradeRequested(boost::asio::yield_context yield) override
    {
        auto expectedRequest = fetch(yield);
        if (not expectedRequest.has_value())
            return std::unexpected{std::move(expectedRequest).error()};

        request_ = std::move(expectedRequest).value();

        return boost::beast::websocket::is_upgrade(*request_);
    }

    std::expected<ConnectionPtr, Error>
    upgrade(
        util::TagDecoratorFactory const& tagDecoratorFactory,
        boost::asio::yield_context yield
    ) override
    {
        ASSERT(request_.has_value(), "Request must be present to upgrade the connection");

        return makeWsConnection(
            std::move(stream_),
            std::move(ip_),
            std::move(buffer_),
            std::move(*request_),  // NOLINT(bugprone-unchecked-optional-access)
            tagDecoratorFactory,
            yield
        );
    }

private:
    std::expected<boost::beast::http::request<boost::beast::http::string_body>, Error>
    fetch(boost::asio::yield_context yield)
    {
        boost::beast::http::request<boost::beast::http::string_body> request{};
        boost::system::error_code error;
        boost::beast::get_lowest_layer(stream_).expires_after(timeout_);
        boost::beast::http::async_read(stream_, buffer_, request, yield[error]);
        if (error)
            return std::unexpected{error};
        return request;
    }
};

using PlainHttpConnection = HttpConnection<boost::beast::tcp_stream>;

using SslHttpConnection = HttpConnection<boost::asio::ssl::stream<boost::beast::tcp_stream>>;

}  // namespace web::ng::impl
