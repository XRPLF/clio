#pragma once

#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>

#include <concepts>
#include <memory>
#include <string>

namespace web {

struct ConnectionBase;

/**
 * @brief Specifies the requirements a Webserver handler must fulfill.
 */
template <typename T>
concept SomeServerHandler = requires(
    T handler,
    std::string req,
    std::shared_ptr<ConnectionBase> ws,
    boost::beast::error_code ec
) {
    // the callback when server receives a request
    { handler(req, ws) };
};

/**
 * @brief A tag class for server to help identify Server in templated code.
 */
struct ServerTag {
    virtual ~ServerTag() = default;
};

template <typename T>
concept SomeServer = std::derived_from<T, ServerTag>;

}  // namespace web
