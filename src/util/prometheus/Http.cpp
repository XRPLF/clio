#include "util/prometheus/Http.hpp"

#include "util/prometheus/Prometheus.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

#include <optional>

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
