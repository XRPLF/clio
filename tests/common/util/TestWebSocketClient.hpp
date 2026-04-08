#pragma once

#include "util/TestHttpClient.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class WebSocketSyncClient {
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::resolver resolver_{ioc_};
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_{ioc_};

public:
    void
    connect(
        std::string const& host,
        std::string const& port,
        std::vector<WebHeader> additionalHeaders = {}
    );

    void
    disconnect();

    std::string
    syncPost(std::string const& body);
};

class WebSocketAsyncClient {
    boost::beast::websocket::stream<boost::beast::tcp_stream> stream_;

public:
    WebSocketAsyncClient(boost::asio::io_context& ioContext);

    std::expected<void, boost::system::error_code>
    connect(
        std::string const& host,
        std::string const& port,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout,
        std::vector<WebHeader> additionalHeaders = {}
    );

    std::expected<void, boost::system::error_code>
    send(
        boost::asio::yield_context yield,
        std::string_view message,
        std::chrono::steady_clock::duration timeout
    );

    std::expected<std::string, boost::system::error_code>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    gracefulClose(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    close();
};

class WebServerSslSyncClient {
    boost::asio::io_context ioc_;
    std::optional<
        boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>>
        ws_;

public:
    void
    connect(std::string const& host, std::string const& port);

    void
    disconnect();

    std::string
    syncPost(std::string const& body);
};
