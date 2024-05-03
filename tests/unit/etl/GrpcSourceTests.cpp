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

#include "etl/impl/GrpcSource.hpp"
#include "util/Fixtures.hpp"
#include "util/MockBackend.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockXrpLedgerAPIService.hpp"
#include "util/TestObject.hpp"
#include "util/config/Config.hpp"

#include <gmock/gmock.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_data.pb.h>
#include <ripple/basics/base_uint.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace etl::impl;

struct GrpcSourceTests : NoLoggerFixture,
                         util::prometheus::WithPrometheus,
                         unittests::util::WithMockXrpLedgerAPIService {
    GrpcSourceTests()
        : WithMockXrpLedgerAPIService("localhost:55051")
        , mockBackend_(std::make_shared<testing::StrictMock<MockBackend>>(util::Config{}))
        , grpcSource_("127.0.0.1", "55051", mockBackend_)
    {
    }

    std::shared_ptr<testing::StrictMock<MockBackend>> mockBackend_;
    testing::StrictMock<GrpcSource> grpcSource_;
};

TEST_F(GrpcSourceTests, fetchLedger)
{
    uint32_t const sequence = 123;
    bool const getObjects = true;
    bool const getObjectNeighbors = false;

    EXPECT_CALL(mockXrpLedgerAPIService, GetLedger)
        .WillOnce([&](grpc::ServerContext* /*context*/,
                      org::xrpl::rpc::v1::GetLedgerRequest const* request,
                      org::xrpl::rpc::v1::GetLedgerResponse* response) {
            EXPECT_EQ(request->ledger().sequence(), sequence);
            EXPECT_TRUE(request->transactions());
            EXPECT_TRUE(request->expand());
            EXPECT_EQ(request->get_objects(), getObjects);
            EXPECT_EQ(request->get_object_neighbors(), getObjectNeighbors);
            EXPECT_EQ(request->user(), "ETL");
            response->set_validated(true);
            response->set_is_unlimited(false);
            response->set_object_neighbors_included(false);
            return grpc::Status{};
        });
    auto const [status, response] = grpcSource_.fetchLedger(sequence, getObjects, getObjectNeighbors);
    ASSERT_TRUE(status.ok());
    EXPECT_TRUE(response.validated());
    EXPECT_FALSE(response.is_unlimited());
    EXPECT_FALSE(response.object_neighbors_included());
}

TEST_F(GrpcSourceTests, fetchLedgerNoStub)
{
    testing::StrictMock<GrpcSource> wrongGrpcSource{"wrong", "wrong", mockBackend_};
    auto const [status, _response] = wrongGrpcSource.fetchLedger(0, false, false);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(GrpcSourceTests, loadInitialLedgerNoStub)
{
    testing::StrictMock<GrpcSource> wrongGrpcSource{"wrong", "wrong", mockBackend_};
    auto const [data, success] = wrongGrpcSource.loadInitialLedger(0, 0, false);
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(success);
}

struct GrpcSourceLoadInitialLedgerTests : GrpcSourceTests {
    uint32_t const sequence_ = 123;
    uint32_t const numMarkers_ = 4;
    bool const cacheOnly_ = false;
};

TEST_F(GrpcSourceLoadInitialLedgerTests, GetLedgerDataFailed)
{
    EXPECT_CALL(mockXrpLedgerAPIService, GetLedgerData)
        .Times(numMarkers_)
        .WillRepeatedly([&](grpc::ServerContext* /*context*/,
                            org::xrpl::rpc::v1::GetLedgerDataRequest const* request,
                            org::xrpl::rpc::v1::GetLedgerDataResponse* /*response*/) {
            EXPECT_EQ(request->ledger().sequence(), sequence_);
            EXPECT_EQ(request->user(), "ETL");
            return grpc::Status{grpc::StatusCode::NOT_FOUND, "Not found"};
        });

    auto const [data, success] = grpcSource_.loadInitialLedger(sequence_, numMarkers_, cacheOnly_);
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(success);
}

TEST_F(GrpcSourceLoadInitialLedgerTests, worksFine)
{
    auto const key = ripple::uint256{4};
    std::string const keyStr{reinterpret_cast<char const*>(key.data()), ripple::uint256::size()};
    auto const object = CreateTicketLedgerObject("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", sequence_);
    auto const objectData = object.getSerializer().peekData();

    EXPECT_CALL(mockXrpLedgerAPIService, GetLedgerData)
        .Times(numMarkers_)
        .WillRepeatedly([&](grpc::ServerContext* /*context*/,
                            org::xrpl::rpc::v1::GetLedgerDataRequest const* request,
                            org::xrpl::rpc::v1::GetLedgerDataResponse* response) {
            EXPECT_EQ(request->ledger().sequence(), sequence_);
            EXPECT_EQ(request->user(), "ETL");

            response->set_is_unlimited(true);
            auto newObject = response->mutable_ledger_objects()->add_objects();
            newObject->set_key(key.data(), ripple::uint256::size());
            newObject->set_data(objectData.data(), objectData.size());

            return grpc::Status{};
        });

    EXPECT_CALL(*mockBackend_, writeNFTs).Times(numMarkers_);
    EXPECT_CALL(*mockBackend_, writeLedgerObject).Times(numMarkers_);

    auto const [data, success] = grpcSource_.loadInitialLedger(sequence_, numMarkers_, cacheOnly_);

    EXPECT_TRUE(success);
    EXPECT_EQ(data, std::vector<std::string>(4, keyStr));
}
