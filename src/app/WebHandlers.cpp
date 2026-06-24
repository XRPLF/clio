#include "app/WebHandlers.hpp"

#include "rpc/Errors.hpp"
#include "rpc/WorkQueue.hpp"
#include "util/Assert.hpp"
#include "util/CoroutineGroup.hpp"
#include "util/prometheus/Http.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/status.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace app {

OnConnectCheck::OnConnectCheck(web::dosguard::DOSGuardInterface& dosguard) : dosguard_{dosguard}
{
}

std::expected<void, web::ng::Response>
OnConnectCheck::operator()(web::ng::Connection const& connection)
{
    dosguard_.get().increment(connection.ip());
    if (not dosguard_.get().isOk(connection.ip())) {
        return std::unexpected{web::ng::Response{
            boost::beast::http::status::too_many_requests, "Too many requests", connection
        }};
    }

    return {};
}

IpChangeHook::IpChangeHook(web::dosguard::DOSGuardInterface& dosguard) : dosguard_(dosguard)
{
}

void
IpChangeHook::operator()(std::string const& oldIp, std::string const& newIp)
{
    dosguard_.get().decrement(oldIp);
    dosguard_.get().increment(newIp);
}

DisconnectHook::DisconnectHook(web::dosguard::DOSGuardInterface& dosguard) : dosguard_{dosguard}
{
}

void
DisconnectHook::operator()(web::ng::Connection const& connection)
{
    dosguard_.get().decrement(connection.ip());
}

MetricsHandler::MetricsHandler(
    std::shared_ptr<web::AdminVerificationStrategy> adminVerifier,
    rpc::WorkQueue& workQueue
)
    : adminVerifier_{std::move(adminVerifier)}, workQueue_{std::ref(workQueue)}
{
}

web::ng::Response
MetricsHandler::operator()(
    web::ng::Request const& request,
    web::ng::ConnectionMetadata& connectionMetadata,
    web::SubscriptionContextPtr,
    boost::asio::yield_context yield
)
{
    std::optional<web::ng::Response> response;
    util::CoroutineGroup coroutineGroup{yield, 1};
    auto const onTaskComplete = coroutineGroup.registerForeign(yield);
    ASSERT(onTaskComplete.has_value(), "Coroutine group can't be full");

    bool const postSuccessful = workQueue_.get().postCoro(
        [this, &request, &response, &onTaskComplete = *onTaskComplete, &connectionMetadata](  // NOLINT(bugprone-unchecked-optional-access)
            boost::asio::yield_context
        ) mutable {
            auto const maybeHttpRequest = request.asHttpRequest();
            ASSERT(maybeHttpRequest.has_value(), "Got not a http request in Get");
            auto const& httpRequest = maybeHttpRequest->get();

            auto maybeResponse = util::prometheus::handlePrometheusRequest(
                httpRequest, adminVerifier_->isAdmin(httpRequest, connectionMetadata.ip())
            );
            ASSERT(maybeResponse.has_value(), "Got unexpected request for Prometheus");
            response = web::ng::Response{*std::move(maybeResponse), request};
            // notify the coroutine group that the foreign task is done
            onTaskComplete();
        },
        /* isWhiteListed= */ true,
        rpc::WorkQueue::Priority::High
    );

    if (!postSuccessful) {
        return web::ng::Response{
            boost::beast::http::status::too_many_requests,
            rpc::makeError(rpc::RippledError::RpcTooBusy),
            request
        };
    }

    // Put the coroutine to sleep until the foreign task is done
    coroutineGroup.asyncWait(yield);
    ASSERT(response.has_value(), "Woke up coroutine without setting response");

    return *std::move(response);  // NOLINT(bugprone-unchecked-optional-access)
}

web::ng::Response
HealthCheckHandler::operator()(
    web::ng::Request const& request,
    web::ng::ConnectionMetadata&,
    web::SubscriptionContextPtr,
    boost::asio::yield_context
)
{
    static constexpr auto kHealthCheckHtml = R"html(
    <!DOCTYPE html>
    <html>
        <head><title>Test page for Clio</title></head>
        <body><h1>Clio Test</h1><p>This page shows Clio http(s) connectivity is working.</p></body>
    </html>
)html";

    return web::ng::Response{boost::beast::http::status::ok, kHealthCheckHtml, request};
}

web::ng::Response
CacheStateHandler::operator()(
    web::ng::Request const& request,
    web::ng::ConnectionMetadata&,
    web::SubscriptionContextPtr,
    boost::asio::yield_context
)
{
    static constexpr auto kCacheCheckLoadedHtml = R"html(
    <!DOCTYPE html>
    <html>
        <head><title>Cache state</title></head>
        <body><h1>Cache state</h1><p>Cache is fully loaded</p></body>
    </html>
)html";

    static constexpr auto kCacheCheckNotLoadedHtml = R"html(
    <!DOCTYPE html>
    <html>
        <head><title>Cache state</title></head>
        <body><h1>Cache state</h1><p>Cache is not yet loaded</p></body>
    </html>
)html";

    if (cache_.get().isFull())
        return web::ng::Response{boost::beast::http::status::ok, kCacheCheckLoadedHtml, request};

    return web::ng::Response{
        boost::beast::http::status::service_unavailable, kCacheCheckNotLoadedHtml, request
    };
}

}  // namespace app
