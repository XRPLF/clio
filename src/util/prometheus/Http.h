//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================
#pragma once

#include <util/prometheus/Prometheus.h>

#include <boost/beast/http.hpp>

namespace util::prometheus {

/**
 * @brief Checks whether request is a prometheus request
 *
 * @param req The http request from the client
 * @param isAdmin Whether the request is from an admin
 * @return true if the request is from prometheus request; false otherwise
 */
bool
isPrometheusRequest(boost::beast::http::request<boost::beast::http::string_body> const& req, bool isAdmin);

/**
 * @brief Handles a prometheus request
 *
 * @param req The http request from primetheus (required only to reply with the same http version)
 * @return The http response containing all the prometheus data from clio
 */
boost::beast::http::response<boost::beast::http::string_body>
handlePrometheusRequest(boost::beast::http::request<boost::beast::http::string_body> const& req);

}  // namespace util::prometheus
