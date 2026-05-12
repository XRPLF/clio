#pragma once

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace web::ng {

/**
 * @brief Represents an HTTP or WebSocket request.
 */
class Request {
public:
    /**
     * @brief The headers of an HTTP request.
     */
    using HttpHeaders = boost::beast::http::request<boost::beast::http::string_body>::header_type;

private:
    struct WsData {
        std::string request;
        std::reference_wrapper<HttpHeaders const> headers;
    };

    using HttpRequest = boost::beast::http::request<boost::beast::http::string_body>;
    std::variant<HttpRequest, WsData> data_;

public:
    /**
     * @brief Construct from an HTTP request.
     *
     * @param request The HTTP request.
     */
    explicit Request(boost::beast::http::request<boost::beast::http::string_body> request);

    /**
     * @brief Construct from a WebSocket request.
     *
     * @param request The WebSocket request.
     * @param headers The headers of the HTTP request initiated the WebSocket connection
     */
    Request(std::string request, HttpHeaders const& headers);

    /**
     * @brief Method of the request.
     * @note Websocket is not a real method, it is used to distinguish WebSocket requests from HTTP
     * requests.
     */
    enum class Method { Get, Post, Websocket, Unsupported };

    /**
     * @brief Get the method of the request.
     *
     * @return The method of the request.
     */
    [[nodiscard]] Method
    method() const;

    /**
     * @brief Check if the request is an HTTP request.
     *
     * @return true if the request is an HTTP request, false otherwise.
     */
    [[nodiscard]] bool
    isHttp() const;

    /**
     * @brief Get the HTTP request.
     *
     * @return The HTTP request or std::nullopt if the request is a WebSocket request.
     */
    [[nodiscard]] std::optional<
        std::reference_wrapper<boost::beast::http::request<boost::beast::http::string_body> const>>
    asHttpRequest() const;

    /**
     * @brief Get the body (in case of an HTTP request) or the message (in case of a WebSocket
     * request).
     *
     * @return The message of the request.
     */
    [[nodiscard]] std::string_view
    message() const;

    /**
     * @brief Get the target of the request.
     *
     * @return The target of the request or std::nullopt if the request is a WebSocket request.
     */
    [[nodiscard]] std::optional<std::string_view>
    target() const;

    /**
     * @brief Get the headers of the request.
     *
     * @return The headers of the request.
     */
    [[nodiscard]] HttpHeaders const&
    httpHeaders() const;

    /**
     * @brief Get the value of a header.
     *
     * @param headerName The name of the header.
     * @return The value of the header or std::nullopt if the header does not exist.
     */
    [[nodiscard]] std::optional<std::string_view>
    headerValue(boost::beast::http::field headerName) const;

    /**
     * @brief Get the value of a header.
     *
     * @param headerName The name of the header.
     * @return The value of the header or std::nullopt if the header does not exist.
     */
    [[nodiscard]] std::optional<std::string_view>
    headerValue(std::string const& headerName) const;

private:
    /**
     * @brief Get the HTTP request.
     * @note This function assumes that the request is an HTTP request. So if data_ is not an HTTP
     * request, the behavior is undefined.
     *
     * @return The HTTP request.
     */
    [[nodiscard]] HttpRequest const&
    httpRequest() const;
};

}  // namespace web::ng
