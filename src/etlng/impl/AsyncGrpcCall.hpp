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

#pragma once

#include "etlng/InitialLoadObserverInterface.hpp"
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

namespace etlng::impl {

class AsyncGrpcCall {
public:
    enum class CallStatus { MORE, DONE, ERRORED };
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
    AsyncGrpcCall(uint32_t seq, ripple::uint256 const& marker, std::optional<ripple::uint256> const& nextMarker);

    static std::vector<AsyncGrpcCall>
    makeAsyncCalls(uint32_t const sequence, uint32_t const numMarkers);

    CallStatus
    process(
        std::unique_ptr<StubType>& stub,
        grpc::CompletionQueue& cq,
        etlng::InitialLoadObserverInterface& loader,
        bool abort
    );

    void
    call(std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub, grpc::CompletionQueue& cq);

    std::string
    getMarkerPrefix();

    std::string
    getLastKey();
};

}  // namespace etlng::impl
