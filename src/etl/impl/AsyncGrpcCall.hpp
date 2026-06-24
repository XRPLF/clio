#pragma once

#include "etl/InitialLoadObserverInterface.hpp"
#include "util/log/Logger.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger_data.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace etl::impl {

class AsyncGrpcCall {
public:
    enum class CallStatus { More, Done, Errored };
    using RequestType = org::xrpl::rpc::v1::GetLedgerDataRequest;
    using ResponseType = org::xrpl::rpc::v1::GetLedgerDataResponse;
    using StubType = org::xrpl::rpc::v1::XRPLedgerAPIService::Stub;

private:
    util::Logger log_{"ETL"};

    std::unique_ptr<ResponseType> cur_;
    std::unique_ptr<ResponseType> next_;

    RequestType request_;
    std::unique_ptr<grpc::ClientContext> context_;

    grpc::Status status_;
    unsigned char nextPrefix_;

    std::string lastKey_;
    std::optional<std::string> predecessorKey_;

public:
    AsyncGrpcCall(
        uint32_t seq,
        xrpl::uint256 const& marker,
        std::optional<xrpl::uint256> const& nextMarker
    );

    static std::vector<AsyncGrpcCall>
    makeAsyncCalls(uint32_t const sequence, uint32_t const numMarkers);

    CallStatus
    process(
        std::unique_ptr<StubType>& stub,
        grpc::CompletionQueue& cq,
        InitialLoadObserverInterface& loader,
        bool abort
    );

    void
    call(
        std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub,
        grpc::CompletionQueue& cq
    );

    std::string
    getMarkerPrefix();

    std::string
    getLastKey();
};

}  // namespace etl::impl
