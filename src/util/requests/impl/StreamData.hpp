#pragma once

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

namespace util::requests::impl {

template <typename StreamType>
struct PlainStreamData {
    static constexpr bool kSslEnabled = false;

    explicit PlainStreamData(boost::asio::yield_context yield)
        : stream(boost::asio::get_associated_executor(yield))
    {
    }

    StreamType stream;
};

using TcpStreamData = PlainStreamData<boost::beast::tcp_stream>;
using WsStreamData = PlainStreamData<boost::beast::websocket::stream<boost::beast::tcp_stream>>;

template <typename StreamType>
struct SslStreamData {
    static constexpr bool kSslEnabled = true;
    StreamType stream;

    static SslStreamData
    create(boost::asio::yield_context yield)
    {
        return SslStreamData{getClientSslContext(), yield};
    }

private:
    SslStreamData(boost::asio::ssl::context& sslContext, boost::asio::yield_context yield)
        : stream(boost::asio::get_associated_executor(yield), sslContext)
    {
    }
};

using SslTcpStreamData = SslStreamData<boost::asio::ssl::stream<boost::beast::tcp_stream>>;
using SslWsStreamData = SslStreamData<
    boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>>;

}  // namespace util::requests::impl
