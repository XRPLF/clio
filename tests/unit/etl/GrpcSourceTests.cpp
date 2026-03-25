#include "data/DBHelpers.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "etl/Models.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/Assert.hpp"
#include "util/MockXrpLedgerAPIService.hpp"
#include "util/Mutex.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <org/xrpl/rpc/v1/get_ledger_data.pb.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
#include <string>
#include <vector>

using namespace etl::model;

namespace {

struct MockLoadObserver : etl::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<Object> const&, std::optional<std::string>),
        (override)
    );
};

struct GrpcSourceTests : virtual public ::testing::Test, tests::util::WithMockXrpLedgerAPIService {
    GrpcSourceTests()
        : WithMockXrpLedgerAPIService("localhost:0")
        , grpcSource_("localhost", std::to_string(getXRPLMockPort()))
    {
    }

    class KeyStore {
        std::vector<ripple::uint256> keys_;
        using Store = std::map<std::string, std::queue<ripple::uint256>, std::greater<>>;

        util::Mutex<Store> store_;

    public:
        KeyStore(std::size_t totalKeys, std::size_t numMarkers) : keys_(etl::getMarkers(totalKeys))
        {
            auto const totalPerMarker = totalKeys / numMarkers;
            auto const markers = etl::getMarkers(numMarkers);

            auto store = store_.lock();
            for (auto mi = 0uz; mi < markers.size(); ++mi) {
                for (auto i = 0uz; i < totalPerMarker; ++i) {
                    auto const mapKey = ripple::strHex(markers.at(mi)).substr(0, 2);
                    store->operator[](mapKey).push(keys_.at((mi * totalPerMarker) + i));
                }
            }
        }

        std::optional<std::string>
        next(std::string const& marker)
        {
            auto store = store_.lock<std::scoped_lock>();

            auto const mapKey = ripple::strHex(marker).substr(0, 2);
            auto it = store->lower_bound(mapKey);
            ASSERT(it != store->end(), "Lower bound not found for '{}'", mapKey);

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
            auto store = store_.lock<std::scoped_lock>();

            auto const mapKey = ripple::strHex(marker).substr(0, 2);
            auto it = store->lower_bound(mapKey);
            ASSERT(it != store->end(), "Lower bound not found for '{}'", mapKey);

            auto& queue = it->second;
            if (queue.empty())
                return std::nullopt;

            auto data = queue.front();
            return std::make_optional(uint256ToString(data));
        };
    };

protected:
    testing::StrictMock<MockLoadObserver> observer_;
    etl::impl::GrpcSource grpcSource_;
};

struct GrpcSourceLoadInitialLedgerTests : GrpcSourceTests {
protected:
    uint32_t const sequence_ = 123u;
    uint32_t const numMarkers_ = 4u;
    bool const cacheOnly_ = false;
};

}  // namespace

TEST_F(GrpcSourceTests, BasicFetchLedger)
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

    auto const [status, response] =
        grpcSource_.fetchLedger(sequence, getObjects, getObjectNeighbors);
    ASSERT_TRUE(status.ok());
    EXPECT_TRUE(response.validated());
    EXPECT_FALSE(response.is_unlimited());
    EXPECT_FALSE(response.object_neighbors_included());
}

TEST_F(GrpcSourceLoadInitialLedgerTests, GetLedgerDataNotFound)
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

    auto const res = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);
    EXPECT_FALSE(res.has_value());
}

TEST_F(GrpcSourceLoadInitialLedgerTests, ObserverCalledCorrectly)
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
        .WillRepeatedly(
            [&](uint32_t, std::vector<Object> const& data, std::optional<std::string> lastKey) {
                EXPECT_FALSE(lastKey.has_value());
                EXPECT_EQ(data.size(), 1);
            }
        );

    auto const res = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value().size(), numMarkers_);

    EXPECT_EQ(res.value(), std::vector<std::string>(4, keyStr));
}

TEST_F(GrpcSourceLoadInitialLedgerTests, DataTransferredAndObserverCalledCorrectly)
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
        .WillRepeatedly(
            [&](uint32_t, std::vector<Object> const& data, std::optional<std::string> lastKey) {
                EXPECT_LE(data.size(), batchSize);

                if (lastKey.has_value()) {
                    ++totalWithLastKey;
                } else {
                    ++totalWithoutLastKey;
                }

                total += data.size();
            }
        );

    auto const res = grpcSource_.loadInitialLedger(sequence_, numMarkers_, observer_);

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value().size(), numMarkers_);
    EXPECT_EQ(total, totalKeys);
    EXPECT_EQ(totalWithLastKey + totalWithoutLastKey, numMarkers_ * batchesPerMarker);
    EXPECT_EQ(totalWithoutLastKey, numMarkers_);
    EXPECT_EQ(totalWithLastKey, (numMarkers_ - 1) * batchesPerMarker);
}

struct GrpcSourceStopTests : GrpcSourceTests, SyncAsioContextTest {};

TEST_F(GrpcSourceStopTests, LoadInitialLedgerStopsWhenRequested)
{
    uint32_t const sequence = 123u;
    uint32_t const numMarkers = 1;

    std::mutex mtx;
    std::condition_variable cvGrpcCallActive;
    std::condition_variable cvStopCalled;
    bool grpcCallIsActive = false;
    bool stopHasBeenCalled = false;

    EXPECT_CALL(mockXrpLedgerAPIService, GetLedgerData)
        .WillOnce([&](grpc::ServerContext*,
                      org::xrpl::rpc::v1::GetLedgerDataRequest const* request,
                      org::xrpl::rpc::v1::GetLedgerDataResponse* response) {
            EXPECT_EQ(request->ledger().sequence(), sequence);
            EXPECT_EQ(request->user(), "ETL");

            {
                std::unique_lock const lk(mtx);
                grpcCallIsActive = true;
            }
            cvGrpcCallActive.notify_one();

            {
                std::unique_lock lk(mtx);
                cvStopCalled.wait(lk, [&] { return stopHasBeenCalled; });
            }

            response->set_is_unlimited(true);
            return grpc::Status::OK;
        });

    EXPECT_CALL(observer_, onInitialLoadGotMoreObjects).Times(0);

    auto loadTask = std::async(std::launch::async, [&]() {
        return grpcSource_.loadInitialLedger(sequence, numMarkers, observer_);
    });

    {
        std::unique_lock lk(mtx);
        cvGrpcCallActive.wait(lk, [&] { return grpcCallIsActive; });
    }

    runSyncOperation([&](boost::asio::yield_context yield) {
        grpcSource_.stop(yield);
        {
            std::unique_lock const lk(mtx);
            stopHasBeenCalled = true;
        }
        cvStopCalled.notify_one();
    });

    auto const res = loadTask.get();

    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), etl::InitialLedgerLoadError::Cancelled);
}

TEST_F(GrpcSourceTests, DeadlineIsHandledCorrectly)
{
    static constexpr auto kDEADLINE = std::chrono::milliseconds{5};

    uint32_t const sequence = 123u;
    bool const getObjects = true;
    bool const getObjectNeighbors = false;

    std::binary_semaphore sem(0);

    auto grpcSource = std::make_unique<etl::impl::GrpcSource>(
        "localhost", std::to_string(getXRPLMockPort()), kDEADLINE
    );

    // Note: this may not be called at all if gRPC cancels before it gets a chance to call the stub
    EXPECT_CALL(mockXrpLedgerAPIService, GetLedger)
        .Times(testing::AtMost(1))
        .WillRepeatedly([&](grpc::ServerContext*,
                            org::xrpl::rpc::v1::GetLedgerRequest const*,
                            org::xrpl::rpc::v1::GetLedgerResponse*) {
            // wait for main thread to discard us and fail the test if unsuccessful within expected
            // timeframe
            [&] { ASSERT_TRUE(sem.try_acquire_for(std::chrono::milliseconds{50})); }();
            return grpc::Status{};
        });

    auto const [status, response] =
        grpcSource->fetchLedger(sequence, getObjects, getObjectNeighbors);
    ASSERT_FALSE(status.ok());  // timed out after kDEADLINE

    sem.release();  // we don't need to hold GetLedger thread any longer
    grpcSource.reset();

    shutdown(std::chrono::milliseconds{10});
}
