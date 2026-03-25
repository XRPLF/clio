#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/verify_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

struct WebHeader {
    WebHeader(boost::beast::http::field name, std::string value);
    WebHeader(std::string_view name, std::string value);

    std::variant<boost::beast::http::field, std::string> name;
    std::string value;
};

struct HttpSyncClient {
    static std::pair<boost::beast::http::status, std::string>
    post(
        std::string const& host,
        std::string const& port,
        std::string const& body,
        std::vector<WebHeader> additionalHeaders = {}
    );

    static std::pair<boost::beast::http::status, std::string>
    get(std::string const& host,
        std::string const& port,
        std::string const& body,
        std::string const& target,
        std::vector<WebHeader> additionalHeaders = {});
};

struct HttpsSyncClient {
    static bool
    verifyCertificate(bool /* preverified */, boost::asio::ssl::verify_context& /* ctx */);

    static std::string
    syncPost(std::string const& host, std::string const& port, std::string const& body);
};

class HttpAsyncClient {
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;

public:
    HttpAsyncClient(boost::asio::io_context& ioContext);

    std::expected<void, boost::system::error_code>
    connect(
        std::string_view host,
        std::string_view port,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout
    );

    std::expected<void, boost::system::error_code>
    send(
        boost::beast::http::request<boost::beast::http::string_body> request,
        boost::asio::yield_context yield,
        std::chrono::steady_clock::duration timeout
    );

    std::expected<
        boost::beast::http::response<boost::beast::http::string_body>,
        boost::system::error_code>
    receive(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout);

    void
    gracefulShutdown();

    void
    disconnect();
};
