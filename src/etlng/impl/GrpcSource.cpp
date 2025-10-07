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

#include "etlng/impl/GrpcSource.hpp"

#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/LoadBalancerInterface.hpp"
#include "etlng/impl/AsyncGrpcCall.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"
#include "web/Resolver.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/format.h>
#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string
resolve(std::string const& ip, std::string const& port)
{
    web::Resolver resolver;

    if (auto const results = resolver.resolve(ip, port); not results.empty())
        return results.at(0);

    throw std::runtime_error("Failed to resolve " + ip + ":" + port);
}

}  // namespace

namespace etlng::impl {

GrpcSource::GrpcSource(std::string const& ip, std::string const& grpcPort, std::chrono::system_clock::duration deadline)
    : log_(fmt::format("ETL_Grpc[{}:{}]", ip, grpcPort))
    , initialLoadShouldStop_(std::make_unique<std::atomic_bool>(false))
    , deadline_{deadline}
{
    try {
        grpc::ChannelArguments chArgs;
        chArgs.SetMaxReceiveMessageSize(-1);
        chArgs.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, kKEEPALIVE_PING_INTERVAL_MS);
        chArgs.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, kKEEPALIVE_TIMEOUT_MS);
        chArgs.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, static_cast<int>(kKEEPALIVE_PERMIT_WITHOUT_CALLS));
        chArgs.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, kMAX_PINGS_WITHOUT_DATA);

        stub_ = org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
            grpc::CreateCustomChannel(resolve(ip, grpcPort), grpc::InsecureChannelCredentials(), chArgs)
        );

        LOG(log_.debug()) << "Made stub for remote.";
    } catch (std::exception const& e) {
        LOG(log_.warn()) << "Exception while creating stub: " << e.what() << ".";
    }
}

std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
GrpcSource::fetchLedger(uint32_t sequence, bool getObjects, bool getObjectNeighbors)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    if (!stub_)
        return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

    org::xrpl::rpc::v1::GetLedgerRequest request;
    grpc::ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + deadline_);  // Prevent indefinite blocking

    request.mutable_ledger()->set_sequence(sequence);
    request.set_transactions(true);
    request.set_expand(true);
    request.set_get_objects(getObjects);
    request.set_get_object_neighbors(getObjectNeighbors);
    request.set_user("ETL");

    grpc::Status const status = stub_->GetLedger(&context, request, &response);

    if (status.ok() and not response.is_unlimited()) {
        log_.warn() << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. Status = "
                    << status.error_message();
    }

    return {status, std::move(response)};
}

InitialLedgerLoadResult
GrpcSource::loadInitialLedger(
    uint32_t const sequence,
    uint32_t const numMarkers,
    etlng::InitialLoadObserverInterface& observer
)
{
    if (*initialLoadShouldStop_)
        return std::unexpected{InitialLedgerLoadError::Cancelled};

    if (!stub_)
        return std::unexpected{InitialLedgerLoadError::Errored};

    std::vector<AsyncGrpcCall> calls = AsyncGrpcCall::makeAsyncCalls(sequence, numMarkers);

    LOG(log_.debug()) << "Starting data download for ledger " << sequence << ".";

    grpc::CompletionQueue queue;
    for (auto& call : calls)
        call.call(stub_, queue);

    std::vector<std::string> edgeKeys;
    void* tag = nullptr;
    bool ok = false;
    bool abort = false;
    size_t numFinished = 0;

    while (numFinished < calls.size() && queue.Next(&tag, &ok)) {
        ASSERT(tag != nullptr, "Tag can't be null.");
        auto ptr = static_cast<AsyncGrpcCall*>(tag);

        if (not ok or *initialLoadShouldStop_) {
            LOG(log_.error()) << "loadInitialLedger cancelled";
            return std::unexpected{InitialLedgerLoadError::Cancelled};
        }

        LOG(log_.trace()) << "Marker prefix = " << ptr->getMarkerPrefix();

        auto result = ptr->process(stub_, queue, observer, abort);
        if (result != AsyncGrpcCall::CallStatus::More) {
            ++numFinished;
            LOG(log_.debug()) << "Finished a marker. Current number of finished = " << numFinished;

            if (auto lastKey = ptr->getLastKey(); !lastKey.empty())
                edgeKeys.push_back(std::move(lastKey));
        }

        if (result == AsyncGrpcCall::CallStatus::Errored)
            abort = true;
    }

    if (abort)
        return std::unexpected{InitialLedgerLoadError::Errored};

    return edgeKeys;
}

void
GrpcSource::stop(boost::asio::yield_context)
{
    initialLoadShouldStop_->store(true);
}

}  // namespace etlng::impl
