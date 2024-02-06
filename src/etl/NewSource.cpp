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

#include "etl/NewSource.hpp"

#include "data/BackendInterface.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/impl/AsyncData.hpp"
#include "feed/SubscriptionManager.hpp"
#include "main/Build.hpp"
#include "util/Assert.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/uuid/random_generator.hpp>
#include <fmt/core.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <ripple/basics/base_uint.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl {

NewSource::NewSource(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> validatedLedgers
)
    : ip_(config.valueOr<std::string>("ip", {}))
    , wsPort_(config.valueOr<std::string>("ws_port", {}))
    , grpcPort_(config.valueOr<std::string>("grpc_port", {}))
    , grpcSource_(ip_, grpcPort_, std::move(backend))
    , subscribedSource_(ioc, ip_, wsPort_, std::move(validatedLedgers), std::move(subscriptions), []() {})
{
    static boost::uuids::random_generator uuidGenerator;
    uuid_ = uuidGenerator();
}

bool
NewSource::hasLedger(uint32_t sequence) const
{
    return subscribedSource_.hasLedger(sequence);
}

std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
NewSource::fetchLedger(uint32_t const sequence, bool const getObjects, bool getObjectNeighbors)
{
    return grpcSource_.fetchLedger(sequence, getObjects, getObjectNeighbors);
}

std::pair<std::vector<std::string>, bool>
NewSource::loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly)
{
    return grpcSource_.loadInitialLedger(sequence, numMarkers, cacheOnly);
}

std::optional<boost::json::object>
NewSource::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& forwardToRippledClientIp,
    boost::asio::yield_context yield
) const
{
    // TODO: add cache
    // TODO: add connection pool
    util::requests::WsConnectionBuilder builder{ip_, wsPort_};
    util::requests::HttpHeader userAgentHeader{
        boost::beast::http::field::user_agent, fmt::format("clio-{}", Build::getClioVersionString())
    };
    builder.setConnectionTimeout(std::chrono::seconds{3});
    builder.addHeader(std::move(userAgentHeader));

    if (forwardToRippledClientIp)
        builder.addHeader({boost::beast::http::field::forwarded, "for=" + *forwardToRippledClientIp});

    auto wsConnection = builder.connect(yield);
    if (not wsConnection.has_value()) {
        LOG(log_.error()) << "Failed to establish ws connection: " << wsConnection.error().message();
        return {};
    }

    // TODO: json may throw so need to put it inside try catch
    auto const error = wsConnection.value()->write(boost::json::serialize(request), yield);
    if (error) {
        LOG(log_.error()) << "Error sending request: " << error->message();
        return {};
    }

    auto const response = wsConnection.value()->read(yield);
    if (not response.has_value()) {
        LOG(log_.error()) << "Error reading response: " << response.error().message();
        return {};
    }

    // TODO: json may throw so need to put it inside try catch
    auto parsedResponse = boost::json::parse(response.value());
    if (not parsedResponse.is_object()) {
        LOG(log_.error()) << "Error parsing response: " << response.value();
        return {};
    }
    auto responseObject = parsedResponse.as_object();
    responseObject["forwarded"] = true;
    return responseObject;
}

}  // namespace etl
