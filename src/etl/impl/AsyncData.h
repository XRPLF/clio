//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <etl/NFTHelpers.h>
#include <util/log/Logger.h>

#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace etl::detail {

class AsyncCallData
{
    util::Logger log_{"ETL"};

    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> cur_;
    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> next_;

    org::xrpl::rpc::v1::GetLedgerDataRequest request_;
    std::unique_ptr<grpc::ClientContext> context_;

    grpc::Status status_;
    unsigned char nextPrefix_;

    std::string lastKey_;

public:
    AsyncCallData(uint32_t seq, ripple::uint256 const& marker, std::optional<ripple::uint256> const& nextMarker)
    {
        request_.mutable_ledger()->set_sequence(seq);
        if (marker.isNonZero())
        {
            request_.set_marker(marker.data(), marker.size());
        }
        request_.set_user("ETL");
        nextPrefix_ = 0x00;
        if (nextMarker)
            nextPrefix_ = nextMarker->data()[0];

        unsigned char prefix = marker.data()[0];

        LOG(log_.debug()) << "Setting up AsyncCallData. marker = " << ripple::strHex(marker)
                          << " . prefix = " << ripple::strHex(std::string(1, prefix))
                          << " . nextPrefix_ = " << ripple::strHex(std::string(1, nextPrefix_));

        assert(nextPrefix_ > prefix || nextPrefix_ == 0x00);

        cur_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();
        next_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();
        context_ = std::make_unique<grpc::ClientContext>();
    }

    enum class CallStatus { MORE, DONE, ERRORED };

    CallStatus
    process(
        std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub,
        grpc::CompletionQueue& cq,
        BackendInterface& backend,
        bool abort,
        bool cacheOnly = false)
    {
        LOG(log_.trace()) << "Processing response. "
                          << "Marker prefix = " << getMarkerPrefix();
        if (abort)
        {
            LOG(log_.error()) << "AsyncCallData aborted";
            return CallStatus::ERRORED;
        }
        if (!status_.ok())
        {
            LOG(log_.error()) << "AsyncCallData status_ not ok: "
                              << " code = " << status_.error_code() << " message = " << status_.error_message();
            return CallStatus::ERRORED;
        }
        if (!next_->is_unlimited())
        {
            LOG(log_.warn()) << "AsyncCallData is_unlimited is false. Make sure "
                                "secure_gateway is set correctly at the ETL source";
        }

        std::swap(cur_, next_);

        bool more = true;

        // if no marker returned, we are done
        if (cur_->marker().size() == 0)
            more = false;

        // if returned marker is greater than our end, we are done
        unsigned char prefix = cur_->marker()[0];
        if (nextPrefix_ != 0x00 && prefix >= nextPrefix_)
            more = false;

        // if we are not done, make the next async call
        if (more)
        {
            request_.set_marker(std::move(cur_->marker()));
            call(stub, cq);
        }

        auto const numObjects = cur_->ledger_objects().objects_size();
        LOG(log_.debug()) << "Writing " << numObjects << " objects";

        std::vector<data::LedgerObject> cacheUpdates;
        cacheUpdates.reserve(numObjects);

        for (int i = 0; i < numObjects; ++i)
        {
            auto& obj = *(cur_->mutable_ledger_objects()->mutable_objects(i));
            if (!more && nextPrefix_ != 0x00)
            {
                if (static_cast<unsigned char>(obj.key()[0]) >= nextPrefix_)
                    continue;
            }
            cacheUpdates.push_back(
                {*ripple::uint256::fromVoidChecked(obj.key()),
                 {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
            if (!cacheOnly)
            {
                if (lastKey_.size())
                    backend.writeSuccessor(std::move(lastKey_), request_.ledger().sequence(), std::string{obj.key()});
                lastKey_ = obj.key();
                backend.writeNFTs(getNFTDataFromObj(request_.ledger().sequence(), obj.key(), obj.data()));
                backend.writeLedgerObject(
                    std::move(*obj.mutable_key()), request_.ledger().sequence(), std::move(*obj.mutable_data()));
            }
        }
        backend.cache().update(cacheUpdates, request_.ledger().sequence(), cacheOnly);
        LOG(log_.debug()) << "Wrote " << numObjects << " objects. Got more: " << (more ? "YES" : "NO");

        return more ? CallStatus::MORE : CallStatus::DONE;
    }

    void
    call(std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub, grpc::CompletionQueue& cq)
    {
        context_ = std::make_unique<grpc::ClientContext>();

        std::unique_ptr<grpc::ClientAsyncResponseReader<org::xrpl::rpc::v1::GetLedgerDataResponse>> rpc(
            stub->PrepareAsyncGetLedgerData(context_.get(), request_, &cq));

        rpc->StartCall();

        rpc->Finish(next_.get(), &status_, this);
    }

    std::string
    getMarkerPrefix()
    {
        if (next_->marker().size() == 0)
            return "";
        else
            return ripple::strHex(std::string{next_->marker().data()[0]});
    }

    std::string
    getLastKey()
    {
        return lastKey_;
    }
};

}  // namespace etl::detail
