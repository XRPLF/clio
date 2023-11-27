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

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include "util/prometheus/Prometheus.h"
#include <optional>
#include <util/prometheus/Http.h>

namespace util::prometheus {

namespace http = boost::beast::http;

namespace {

bool
isPrometheusRequest(http::request<http::string_body> const& req)
{
    return req.method() == http::verb::get && req.target() == "/metrics";
}

}  // namespace

std::optional<http::response<http::string_body>>
handlePrometheusRequest(http::request<http::string_body> const& req, bool const isAdmin)
{
    bool const prometheusRequest = isPrometheusRequest(req);

    if (!prometheusRequest)
        return std::nullopt;

    if (!isAdmin) {
        return http::response<http::string_body>(
            http::status::unauthorized, req.version(), "Only admin is allowed to collect metrics"
        );
    }

    if (not PrometheusService::isEnabled()) {
        return http::response<http::string_body>(
            http::status::forbidden, req.version(), "Prometheus is disabled in clio config"
        );
    }

    auto response = http::response<http::string_body>(http::status::ok, req.version());

    response.set(http::field::content_type, "text/plain; version=0.0.4");

    response.body() = PrometheusService::collectMetrics();

    if (PrometheusService::compressReplyEnabled())
        response.set(http::field::content_encoding, "gzip");

    return response;
}

}  // namespace util::prometheus
