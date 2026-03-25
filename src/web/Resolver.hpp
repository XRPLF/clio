#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace web {

/**
 * @brief The requirements of a resolver
 */
template <typename T>
concept SomeResolver = requires(T t) {
    std::is_default_constructible_v<T>;
    { t.resolve(std::string_view{}, std::string_view{}) } -> std::same_as<std::vector<std::string>>;
    { t.resolve(std::string_view{}) } -> std::same_as<std::vector<std::string>>;
};

/**
 * @brief Simple hostnames to IP addresses resolver.
 */
class Resolver {
    boost::asio::io_context ioContext_;
    boost::asio::ip::tcp::resolver resolver_{ioContext_};

public:
    /**
     * @brief Resolve hostname to IP addresses.
     *
     * @throw This method throws an exception when the hostname cannot be resolved.
     *
     * @param hostname Hostname to resolve
     * @return IP addresses of the hostname
     */
    std::vector<std::string>
    resolve(std::string_view hostname);

    /**
     * @brief Resolve to IP addresses with port.
     *
     * @throw This method throws an exception when the hostname cannot be resolved.
     *
     * @param hostname Hostname to resolve
     * @param service Service to resolve
     * @return IP addresses of the hostname
     */
    std::vector<std::string>
    resolve(std::string_view hostname, std::string_view service);

private:
    std::vector<boost::asio::ip::tcp::endpoint>
    doResolve(std::string_view hostname, std::string_view service);
};

}  // namespace web
