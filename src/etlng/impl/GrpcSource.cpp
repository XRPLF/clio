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
#include "etlng/impl/AsyncGrpcCall.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"
#include "web/Resolver.hpp"

#include <fmt/core.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <cstddef>
#include <cstdint>
#include <exception>
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

GrpcSource::GrpcSource(std::string const& ip, std::string const& grpcPort)
    : log_(fmt::format("ETL_Grpc[{}:{}]", ip, grpcPort))
{
    try {
        grpc::ChannelArguments chArgs;
        chArgs.SetMaxReceiveMessageSize(-1);

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

    if (status.ok() and not response.is_unlimited()) {
        log_.warn() << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. Status = "
                    << status.error_message();
    }

    return {status, std::move(response)};
}

std::pair<std::vector<std::string>, bool>
GrpcSource::loadInitialLedger(
    uint32_t const sequence,
    uint32_t const numMarkers,
    etlng::InitialLoadObserverInterface& observer
)
{
    if (!stub_)
        return {{}, false};

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

        if (!ok) {
            LOG(log_.error()) << "loadInitialLedger - ok is false";
            return {{}, false};  // cancelled
        }

        LOG(log_.trace()) << "Marker prefix = " << ptr->getMarkerPrefix();

        auto result = ptr->process(stub_, queue, observer, abort);
        if (result != AsyncGrpcCall::CallStatus::MORE) {
            ++numFinished;
            LOG(log_.debug()) << "Finished a marker. Current number of finished = " << numFinished;

            if (auto lastKey = ptr->getLastKey(); !lastKey.empty())
                edgeKeys.push_back(std::move(lastKey));
        }

        if (result == AsyncGrpcCall::CallStatus::ERRORED)
            abort = true;
    }

    return {std::move(edgeKeys), !abort};
}

}  // namespace etlng::impl
