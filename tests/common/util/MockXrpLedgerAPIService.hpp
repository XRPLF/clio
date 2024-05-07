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

#include <gmock/gmock.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_data.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_diff.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_entry.pb.h>
#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <memory>
#include <string>
#include <thread>

namespace tests::util {

struct MockXrpLedgerAPIService final : public org::xrpl::rpc::v1::XRPLedgerAPIService::Service {
    ~MockXrpLedgerAPIService() override = default;

    MOCK_METHOD(
        grpc::Status,
        GetLedger,
        (grpc::ServerContext * context,
         org::xrpl::rpc::v1::GetLedgerRequest const* request,
         org::xrpl::rpc::v1::GetLedgerResponse* response),
        (override)
    );

    MOCK_METHOD(
        grpc::Status,
        GetLedgerEntry,
        (grpc::ServerContext * context,
         org::xrpl::rpc::v1::GetLedgerEntryRequest const* request,
         org::xrpl::rpc::v1::GetLedgerEntryResponse* response),
        (override)
    );

    MOCK_METHOD(
        grpc::Status,
        GetLedgerData,
        (grpc::ServerContext * context,
         org::xrpl::rpc::v1::GetLedgerDataRequest const* request,
         org::xrpl::rpc::v1::GetLedgerDataResponse* response),
        (override)
    );

    MOCK_METHOD(
        grpc::Status,
        GetLedgerDiff,
        (grpc::ServerContext * context,
         org::xrpl::rpc::v1::GetLedgerDiffRequest const* request,
         org::xrpl::rpc::v1::GetLedgerDiffResponse* response),
        (override)
    );
};

struct WithMockXrpLedgerAPIService : virtual ::testing::Test {
    WithMockXrpLedgerAPIService(std::string serverAddress)
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
        builder.RegisterService(&mockXrpLedgerAPIService);
        server_ = builder.BuildAndStart();
        serverThread_ = std::thread([this] { server_->Wait(); });
    }

    ~WithMockXrpLedgerAPIService() override
    {
        server_->Shutdown();
        serverThread_.join();
    }

    MockXrpLedgerAPIService mockXrpLedgerAPIService;

private:
    std::unique_ptr<grpc::Server> server_;
    std::thread serverThread_;
};

}  // namespace tests::util
