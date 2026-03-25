#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <optional>

namespace util::prometheus {

/**
 * @brief Handles a prometheus request
 *
 * @param req The http request from primetheus (required only to reply with the same http version)
 * @param isAdmin Whether the request is from an admin
 * @return nullopt if the request shouldn't be handled, respoce for Prometheus otherwise
 */
std::optional<boost::beast::http::response<boost::beast::http::string_body>>
handlePrometheusRequest(
    boost::beast::http::request<boost::beast::http::string_body> const& req,
    bool isAdmin
);

}  // namespace util::prometheus
