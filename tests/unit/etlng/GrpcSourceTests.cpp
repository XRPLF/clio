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

#include "data/DBHelpers.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/impl/GrpcSource.hpp"
#include "util/Assert.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockXrpLedgerAPIService.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_data.pb.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

using namespace etlng::model;

namespace {

struct MockLoadObserver : etlng::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<Object> const&, std::optional<std::string>),
        (override)
    );
};

struct GrpcSourceNgTests : NoLoggerFixture, tests::util::WithMockXrpLedgerAPIService {
    GrpcSourceNgTests()
        : WithMockXrpLedgerAPIService("localhost:0"), grpcSource_("localhost", std::to_string(getXRPLMockPort()))
    {
    }

    class KeyStore {
        std::vector<ripple::uint256> keys_;
        std::map<std::string, std::queue<ripple::uint256>, std::greater<>> store_;

        std::mutex mtx_;

    public:
        KeyStore(std::size_t totalKeys, std::size_t numMarkers) : keys_(etl::getMarkers(totalKeys))
        {
            auto const totalPerMarker = totalKeys / numMarkers;
            auto const markers = etl::getMarkers(numMarkers);

            for (auto mi = 0uz; mi < markers.size(); ++mi) {
                for (auto i = 0uz; i < totalPerMarker; ++i) {
                    auto const mapKey = ripple::strHex(markers.at(mi)).substr(0, 2);
                    store_[mapKey].push(keys_.at((mi * totalPerMarker) + i));
                }
            }
        }

        std::optional<std::string>
        next(std::string const& marker)
        {
            std::scoped_lock const lock(mtx_);

            auto const mapKey = ripple::strHex(marker).substr(0, 2);
            auto it = store_.lower_bound(mapKey);
            ASSERT(it != store_.end(), "Lower bound not found for '{}'", mapKey);

            auto& queue = it->second;
            if (queue.empty())
                return std::nullopt;

            auto data = queue.front();
            queue.pop();

            return std::make_optional(uint256ToString(data));
        };

        std::optional<std::string>
        peek(std::string const& marker)
        {
            std::scoped_lock const lock(mtx_);

            auto const mapKey = ripple::strHex(marker).substr(0, 2);
            auto it = store_.lower_bound(mapKey);
            ASSERT(it != store_.end(), "Lower bound not found for '{}'", mapKey);

            auto& queue = it->second;
            if (queue.empty())
                return std::nullopt;

            auto data = queue.front();
            return std::make_optional(uint256ToString(data));
        };
    };

protected:
    testing::StrictMock<MockLoadObserver> observer_;
    etlng::impl::GrpcSource grpcSource_;
};

struct GrpcSourceNgLoadInitialLedgerTests : GrpcSourceNgTests {
protected:
    uint32_t const sequence_ = 123u;
    uint32_t const numMarkers_ = 4u;
    bool const cacheOnly_ = false;
};

}  // namespace

TEST_F(GrpcSourceNgTests, BasicFetchLedger)
{
    uint32_t const sequence = 123u;
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

TEST_F(GrpcSourceNgLoadInitialLedgerTests, GetLedgerDataNotFound)
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

    auto const [data, success] = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(success);
}

TEST_F(GrpcSourceNgLoadInitialLedgerTests, ObserverCalledCorrectly)
{
    auto const key = ripple::uint256{4};
    auto const keyStr = uint256ToString(key);
    auto const object = createTicketLedgerObject("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", sequence_);
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
            newObject->set_key(uint256ToString(key));
            newObject->set_data(objectData.data(), objectData.size());

            return grpc::Status{};
        });

    EXPECT_CALL(observer_, onInitialLoadGotMoreObjects)
        .Times(numMarkers_)
        .WillRepeatedly([&](uint32_t, std::vector<Object> const& data, std::optional<std::string> lastKey) {
            EXPECT_FALSE(lastKey.has_value());
            EXPECT_EQ(data.size(), 1);
        });

    auto const [data, success] = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(data.size(), numMarkers_);

    EXPECT_EQ(data, std::vector<std::string>(4, keyStr));
}

TEST_F(GrpcSourceNgLoadInitialLedgerTests, DataTransferredAndObserverCalledCorrectly)
{
    auto const totalKeys = 256uz;
    auto const totalPerMarker = totalKeys / numMarkers_;
    auto const batchSize = totalPerMarker / 4uz;
    auto const batchesPerMarker = totalPerMarker / batchSize;

    auto keyStore = KeyStore(totalKeys, numMarkers_);

    auto const object = createTicketLedgerObject("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", sequence_);
    auto const objectData = object.getSerializer().peekData();

    EXPECT_CALL(mockXrpLedgerAPIService, GetLedgerData)
        .Times(numMarkers_ * batchesPerMarker)
        .WillRepeatedly([&](grpc::ServerContext* /*context*/,
                            org::xrpl::rpc::v1::GetLedgerDataRequest const* request,
                            org::xrpl::rpc::v1::GetLedgerDataResponse* response) {
            EXPECT_EQ(request->ledger().sequence(), sequence_);
            EXPECT_EQ(request->user(), "ETL");

            response->set_is_unlimited(true);

            auto next = request->marker().empty() ? std::string("00") : request->marker();
            for (auto i = 0uz; i < batchSize; ++i) {
                if (auto maybeLastKey = keyStore.next(next); maybeLastKey.has_value()) {
                    next = *maybeLastKey;

                    auto newObject = response->mutable_ledger_objects()->add_objects();
                    newObject->set_key(next);
                    newObject->set_data(objectData.data(), objectData.size());
                }
            }

            if (auto maybeNext = keyStore.peek(next); maybeNext.has_value())
                response->set_marker(*maybeNext);

            return grpc::Status::OK;
        });

    std::atomic_size_t total = 0uz;
    std::atomic_size_t totalWithLastKey = 0uz;
    std::atomic_size_t totalWithoutLastKey = 0uz;

    EXPECT_CALL(observer_, onInitialLoadGotMoreObjects)
        .Times(numMarkers_ * batchesPerMarker)
        .WillRepeatedly([&](uint32_t, std::vector<Object> const& data, std::optional<std::string> lastKey) {
            EXPECT_LE(data.size(), batchSize);

            if (lastKey.has_value()) {
                ++totalWithLastKey;
            } else {
                ++totalWithoutLastKey;
            }

            total += data.size();
        });

    auto const [data, success] = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(data.size(), numMarkers_);
    EXPECT_EQ(total, totalKeys);
    EXPECT_EQ(totalWithLastKey + totalWithoutLastKey, numMarkers_ * batchesPerMarker);
    EXPECT_EQ(totalWithoutLastKey, numMarkers_);
    EXPECT_EQ(totalWithLastKey, (numMarkers_ - 1) * batchesPerMarker);
}
