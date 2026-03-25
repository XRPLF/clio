#pragma once

#include "util/requests/Types.hpp"
#include "util/requests/impl/SslContext.hpp"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <expected>
#include <utility>

namespace util::requests::impl {

template <typename StreamType>
struct PlainStreamData {
    static constexpr bool kSSL_ENABLED = false;

    explicit PlainStreamData(boost::asio::yield_context yield)
        : stream(boost::asio::get_associated_executor(yield))
    {
    }

    StreamType stream;
};

using TcpStreamData = PlainStreamData<boost::beast::tcp_stream>;
using WsStreamData = PlainStreamData<boost::beast::websocket::stream<boost::beast::tcp_stream>>;

template <typename StreamType>
class SslStreamData {
    boost::asio::ssl::context sslContext_;

public:
    static constexpr bool kSSL_ENABLED = true;

    static std::expected<SslStreamData, RequestError>
    create(boost::asio::yield_context yield)
    {
        auto sslContext = makeClientSslContext();
        if (not sslContext.has_value()) {
            return std::unexpected{std::move(sslContext.error())};
        }
        return SslStreamData{std::move(sslContext).value(), yield};
    }

    StreamType stream;

private:
    SslStreamData(boost::asio::ssl::context sslContext, boost::asio::yield_context yield)
        : sslContext_(std::move(sslContext))
        , stream(boost::asio::get_associated_executor(yield), sslContext_)

    {
    }
};

using SslTcpStreamData = SslStreamData<boost::asio::ssl::stream<boost::beast::tcp_stream>>;
using SslWsStreamData = SslStreamData<
    boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>>;

}  // namespace util::requests::impl
