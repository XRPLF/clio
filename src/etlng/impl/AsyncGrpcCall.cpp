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

#include "etlng/impl/AsyncGrpcCall.hpp"

#include "etl/ETLHelpers.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/impl/Extraction.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etlng::impl {

AsyncGrpcCall::AsyncGrpcCall(
    uint32_t seq,
    ripple::uint256 const& marker,
    std::optional<ripple::uint256> const& nextMarker
)
{
    request_.set_user("ETL");
    request_.mutable_ledger()->set_sequence(seq);

    if (marker.isNonZero())
        request_.set_marker(marker.data(), ripple::uint256::size());

    nextPrefix_ = nextMarker ? nextMarker->data()[0] : 0x00;
    auto const prefix = marker.data()[0];

    LOG(log_.debug()) << "Setting up AsyncGrpcCall. marker = " << ripple::strHex(marker)
                      << ". prefix = " << ripple::strHex(std::string(1, prefix))
                      << ". nextPrefix_ = " << ripple::strHex(std::string(1, nextPrefix_));

    ASSERT(
        nextPrefix_ > prefix or nextPrefix_ == 0x00,
        "Next prefix must be greater than current prefix. Got: nextPrefix_ = {}, prefix = {}",
        nextPrefix_,
        prefix
    );

    cur_ = std::make_unique<ResponseType>();
    next_ = std::make_unique<ResponseType>();
    context_ = std::make_unique<grpc::ClientContext>();
}

AsyncGrpcCall::CallStatus
AsyncGrpcCall::process(
    std::unique_ptr<AsyncGrpcCall::StubType>& stub,
    grpc::CompletionQueue& cq,
    etlng::InitialLoadObserverInterface& loader,
    bool abort
)
{
    LOG(log_.trace()) << "Processing response. "
                      << "Marker prefix = " << getMarkerPrefix();

    if (abort) {
        LOG(log_.error()) << "AsyncGrpcCall aborted";
        return CallStatus::ERRORED;
    }

    if (!status_.ok()) {
        LOG(log_.error()) << "AsyncGrpcCall status_ not ok: code = " << status_.error_code()
                          << " message = " << status_.error_message();

        return CallStatus::ERRORED;
    }

    if (!next_->is_unlimited()) {
        LOG(log_.warn()) << "AsyncGrpcCall is_unlimited is false. "
                         << "Make sure secure_gateway is set correctly at the ETL source";
    }

    std::swap(cur_, next_);
    auto more = true;

    // if no marker returned, we are done
    if (cur_->marker().empty())
        more = false;

    // if returned marker is greater than our end, we are done
    auto const prefix = cur_->marker()[0];
    if (nextPrefix_ != 0x00 && prefix >= nextPrefix_)
        more = false;

    // if we are not done, make the next async call
    if (more) {
        request_.set_marker(cur_->marker());
        call(stub, cq);
    }

    auto const numObjects = cur_->ledger_objects().objects_size();
    std::vector<etlng::model::Object> data;
    data.reserve(numObjects);

    for (int i = 0; i < numObjects; ++i) {
        auto obj = std::move(*(cur_->mutable_ledger_objects()->mutable_objects(i)));
        if (!more && nextPrefix_ != 0x00) {
            if (static_cast<unsigned char>(obj.key()[0]) >= nextPrefix_)
                continue;
        }

        lastKey_ = obj.key();  // this will end up the last key we actually touched eventually
        data.push_back(etlng::impl::extractObj(std::move(obj)));
    }

    if (not data.empty())
        loader.onInitialLoadGotMoreObjects(request_.ledger().sequence(), data, predecessorKey_);

    predecessorKey_ = lastKey_;  // but for ongoing onInitialObjects calls we need to pass along the key we left
                                 // off at so that we can link the two lists correctly

    return more ? CallStatus::MORE : CallStatus::DONE;
}

void
AsyncGrpcCall::call(std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub, grpc::CompletionQueue& cq)
{
    context_ = std::make_unique<grpc::ClientContext>();
    auto rpc = stub->PrepareAsyncGetLedgerData(context_.get(), request_, &cq);

    rpc->StartCall();
    rpc->Finish(next_.get(), &status_, this);
}

std::string
AsyncGrpcCall::getMarkerPrefix()
{
    return next_->marker().empty() ? std::string{} : ripple::strHex(std::string{next_->marker().data()[0]});
}

// this is used to generate edgeKeys - keys that were the last one in the onInitialObjects list
// then we write them all in one go getting the successor from the cache once it's full
std::string
AsyncGrpcCall::getLastKey()
{
    return lastKey_;
}

std::vector<AsyncGrpcCall>
AsyncGrpcCall::makeAsyncCalls(uint32_t const sequence, uint32_t const numMarkers)
{
    auto const markers = etl::getMarkers(numMarkers);

    std::vector<AsyncGrpcCall> result;
    result.reserve(markers.size());

    for (size_t i = 0; i + 1 < markers.size(); ++i)
        result.emplace_back(sequence, markers[i], markers[i + 1]);

    if (not markers.empty())
        result.emplace_back(sequence, markers.back(), std::nullopt);

    return result;
}

}  // namespace etlng::impl
