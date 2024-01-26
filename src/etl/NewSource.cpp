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

#include "etl/NewSource.h"

#include "data/BackendInterface.h"
#include "etl/ETLHelpers.h"
#include "etl/impl/AsyncData.h"
#include "feed/SubscriptionManager.h"
#include "main/Build.h"
#include "util/Assert.h"
#include "util/config/Config.h"
#include "util/log/Logger.h"
#include "util/requests/Types.h"
#include "util/requests/WsConnection.h"

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
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
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
    : backend_(std::move(backend)), networkValidatedLedgers_(std::move(validatedLedgers))
{
    static boost::uuids::random_generator uuidGenerator;
    uuid_ = uuidGenerator();

    ip_ = config.valueOr<std::string>("ip", {});
    wsPort_ = config.valueOr<std::string>("ws_port", {});

    if (auto value = config.maybeValue<std::string>("grpc_port"); value) {
        auto const grpcPort = *value;
        try {
            boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::make_address(ip_), std::stoi(grpcPort)};
            std::stringstream ss;
            ss << endpoint;
            grpc::ChannelArguments chArgs;
            chArgs.SetMaxReceiveMessageSize(-1);
            stub_ = org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
                grpc::CreateCustomChannel(ss.str(), grpc::InsecureChannelCredentials(), chArgs)
            );
            LOG(log_.debug()) << "Made stub for remote = " << toString();
        } catch (std::exception const& e) {
            LOG(log_.warn()) << "Exception while creating stub = " << e.what() << " . Remote = " << toString();
        }
    }
}

bool
NewSource::hasLedger(uint32_t sequence) const
{
    auto validatedLedgers = validatedLedgers_.lock();
    for (auto& pair : validatedLedgers.get()) {
        if (sequence >= pair.first && sequence <= pair.second) {
            return true;
        }
        if (sequence < pair.first) {
            // validatedLedgers_ is a sorted list of disjoint ranges
            // if the sequence comes before this range, the sequence will
            // come before all subsequent ranges
            return false;
        }
    }
    return false;
}

std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
NewSource::fetchLedger(uint32_t sequence, bool getObjects, bool getObjectNeighbors)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    if (!stub_)
        return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

    // Ledger header with txns and metadata
    org::xrpl::rpc::v1::GetLedgerRequest request;
    grpc::ClientContext context;

    request.mutable_ledger()->set_sequence(sequence);
    request.set_transactions(true);
    request.set_expand(true);
    request.set_get_objects(getObjects);
    request.set_get_object_neighbors(getObjectNeighbors);
    request.set_user("ETL");

    grpc::Status const status = stub_->GetLedger(&context, request, &response);

    if (status.ok() && !response.is_unlimited()) {
        log_.warn() << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. source = "
                    << toString() << "; status = " << status.error_message();
    }

    return {status, std::move(response)};
}

std::pair<std::vector<std::string>, bool>
NewSource::loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly)
{
    if (!stub_)
        return {{}, false};

    std::vector<etl::detail::AsyncCallData> calls = detail::makeAsyncCallData(sequence, numMarkers);

    LOG(log_.debug()) << "Starting data download for ledger " << sequence << ". Using source = " << toString();

    grpc::CompletionQueue cq;
    for (auto& c : calls)
        c.call(stub_, cq);

    void* tag = nullptr;
    bool ok = false;
    size_t numFinished = 0;
    bool abort = false;
    size_t const incr = 500000;
    size_t progress = incr;
    std::vector<std::string> edgeKeys;

    while (numFinished < calls.size() && cq.Next(&tag, &ok)) {
        ASSERT(tag != nullptr, "Tag can't be null.");
        auto ptr = static_cast<etl::detail::AsyncCallData*>(tag);

        if (!ok) {
            LOG(log_.error()) << "loadInitialLedger - ok is false";
            return {{}, false};  // handle cancelled
        }

        LOG(log_.trace()) << "Marker prefix = " << ptr->getMarkerPrefix();

        auto result = ptr->process(stub_, cq, *backend_, abort, cacheOnly);
        if (result != etl::detail::AsyncCallData::CallStatus::MORE) {
            ++numFinished;
            LOG(log_.debug()) << "Finished a marker. "
                              << "Current number of finished = " << numFinished;

            std::string const lastKey = ptr->getLastKey();

            if (!lastKey.empty())
                edgeKeys.push_back(ptr->getLastKey());
        }

        if (result == etl::detail::AsyncCallData::CallStatus::ERRORED)
            abort = true;

        if (backend_->cache().size() > progress) {
            LOG(log_.info()) << "Downloaded " << backend_->cache().size() << " records from rippled";
            progress += incr;
        }
    }

    LOG(log_.info()) << "Finished loadInitialLedger. cache size = " << backend_->cache().size();
    return {std::move(edgeKeys), !abort};
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
    builder.setTimeout(std::chrono::seconds{3});
    builder.addHeader(std::move(userAgentHeader));

    if (forwardToRippledClientIp)
        builder.addHeader({boost::beast::http::field::forwarded, "for=" + *forwardToRippledClientIp});

    auto wsConnection = builder.connect(yield);
    if (not wsConnection.has_value()) {
        LOG(log_.error()) << "Failed to establish ws connection: " << wsConnection.error();
        return {};
    }

    // TODO: json may throw so need to put it inside try catch
    auto const error = wsConnection.value()->write(boost::json::serialize(request), yield);
    if (error) {
        LOG(log_.error()) << "Error sending request: " << error->message;
        return {};
    }

    auto const response = wsConnection.value()->read(yield);
    if (not response.has_value()) {
        LOG(log_.error()) << "Error reading response: " << response.error().message;
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
