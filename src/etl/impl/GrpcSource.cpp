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

#include "etl/impl/GrpcSource.h"

#include "data/BackendInterface.h"
#include "etl/impl/AsyncData.h"
#include "util/Assert.h"
#include "util/config/Config.h"
#include "util/log/Logger.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

GrpcSource::GrpcSource(util::Config const& config, std::shared_ptr<BackendInterface> backend)
    : backend_(std::move(backend))
{
    auto const ip = config.valueOr<std::string>("ip", {});
    if (auto value = config.maybeValue<std::string>("grpc_port"); value) {
        auto const grpcPort = *value;
        try {
            boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::make_address(ip), std::stoi(grpcPort)};
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

    if (status.ok() && !response.is_unlimited()) {
        log_.warn() << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. source = "
                    << toString() << "; status = " << status.error_message();
    }

    return {status, std::move(response)};
}

std::pair<std::vector<std::string>, bool>
GrpcSource::loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly)
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

std::string
GrpcSource::toString() const
{
    // TODO
    return {};
}

}  // namespace etl::impl
