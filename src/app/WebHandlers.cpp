//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "app/WebHandlers.hpp"

#include "util/Assert.hpp"
#include "util/prometheus/Http.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/Connection.hpp"
#include "web/Request.hpp"
#include "web/Response.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/status.hpp>

#include <memory>
#include <optional>
#include <utility>

namespace app {

OnConnectCheck::OnConnectCheck(web::dosguard::DOSGuardInterface& dosguard) : dosguard_{dosguard}
{
}

std::expected<void, web::Response>
OnConnectCheck::operator()(web::Connection const& connection)
{
    dosguard_.get().increment(connection.ip());
    if (not dosguard_.get().isOk(connection.ip())) {
        return std::unexpected{
            web::Response{boost::beast::http::status::too_many_requests, "Too many requests", connection}
        };
    }

    return {};
}

DisconnectHook::DisconnectHook(web::dosguard::DOSGuardInterface& dosguard) : dosguard_{dosguard}
{
}

void
DisconnectHook::operator()(web::Connection const& connection)
{
    dosguard_.get().decrement(connection.ip());
}

MetricsHandler::MetricsHandler(std::shared_ptr<web::AdminVerificationStrategy> adminVerifier)
    : adminVerifier_{std::move(adminVerifier)}
{
}

web::Response
MetricsHandler::operator()(
    web::Request const& request,
    web::ConnectionMetadata& connectionMetadata,
    web::SubscriptionContextPtr,
    boost::asio::yield_context
)
{
    auto const maybeHttpRequest = request.asHttpRequest();
    ASSERT(maybeHttpRequest.has_value(), "Got not a http request in Get");
    auto const& httpRequest = maybeHttpRequest->get();

    // FIXME(#1702): Using veb server thread to handle prometheus request. Better to post on work queue.
    auto maybeResponse = util::prometheus::handlePrometheusRequest(
        httpRequest, adminVerifier_->isAdmin(httpRequest, connectionMetadata.ip())
    );
    ASSERT(maybeResponse.has_value(), "Got unexpected request for Prometheus");
    return web::Response{std::move(maybeResponse).value(), request};
}

web::Response
HealthCheckHandler::operator()(
    web::Request const& request,
    web::ConnectionMetadata&,
    web::SubscriptionContextPtr,
    boost::asio::yield_context
)
{
    static auto constexpr kHEALTH_CHECK_HTML = R"html(
    <!DOCTYPE html>
    <html>
        <head><title>Test page for Clio</title></head>
        <body><h1>Clio Test</h1><p>This page shows Clio http(s) connectivity is working.</p></body>
    </html>
)html";

    return web::Response{boost::beast::http::status::ok, kHEALTH_CHECK_HTML, request};
}

}  // namespace app
