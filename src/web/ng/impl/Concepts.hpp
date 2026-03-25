#pragma once

#include <boost/beast/core/basic_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <type_traits>

namespace web::ng::impl {

template <typename T>
concept IsTcpStream = std::is_same_v<std::decay_t<T>, boost::beast::tcp_stream>;

template <typename T>
concept IsSslTcpStream =
    std::is_same_v<std::decay_t<T>, boost::asio::ssl::stream<boost::beast::tcp_stream>>;

}  // namespace web::ng::impl
