#include "data/Types.hpp"
#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/Models.hpp"
#include "etl/RegistryInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/Loading.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockAssert.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLServiceTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace etl::model;
using namespace etl::impl;
using namespace data;

namespace {

constinit auto const kLEDGER_HASH =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kSEQ = 30;

struct MockRegistry : etl::RegistryInterface {
    MOCK_METHOD(
        void,
        dispatchInitialObjects,
        (uint32_t, std::vector<Object> const&, std::string),
        (override)
    );
    MOCK_METHOD(void, dispatchInitialData, (LedgerData const&), (override));
    MOCK_METHOD(void, dispatch, (LedgerData const&), (override));
};

struct MockLoadObserver : etl::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<Object> const&, std::optional<std::string>),
        (override)
    );
};

struct LoadingTests : util::prometheus::WithPrometheus,
                      MockBackendTest,
                      MockAmendmentBlockHandlerTest {
protected:
    std::shared_ptr<MockRegistry> mockRegistryPtr_ = std::make_shared<MockRegistry>();
    std::shared_ptr<etl::SystemState> state_ = std::make_shared<etl::SystemState>();
    Loader loader_{backend_, mockRegistryPtr_, mockAmendmentBlockHandlerPtr_, state_};
};

struct LoadingAssertTest : common::util::WithMockAssert, LoadingTests {};

auto
createTestData()
{
    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return LedgerData{
        .transactions = {},
        .objects = {util::createObject(), util::createObject(), util::createObject()},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

}  // namespace

TEST_F(LoadingTests, LoadInitialLedger)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_))
        .WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*backend_, doFinishWrites());
    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialData(data));

    auto const res = loader_.loadInitialLedger(data);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(
        rpc::ledgerHeaderToBlob(res.value(), true), rpc::ledgerHeaderToBlob(data.header, true)
    );
}

TEST_F(LoadingTests, LoadSuccess)
{
    state_->isWriting = true;  // writer is active
    auto const data = createTestData();

    EXPECT_CALL(*backend_, doFinishWrites());
    EXPECT_CALL(*mockRegistryPtr_, dispatch(data));

    loader_.load(data);
}

TEST_F(LoadingTests, LoadFailure)
{
    state_->isWriting = true;  // writer is active
    auto const data = createTestData();

    EXPECT_CALL(*backend_, doFinishWrites()).Times(0);
    EXPECT_CALL(*mockRegistryPtr_, dispatch(data)).WillOnce([](auto const&) {
        throw std::runtime_error("some error");
    });
    EXPECT_CALL(*mockAmendmentBlockHandlerPtr_, notifyAmendmentBlocked());

    loader_.load(data);
}

TEST_F(LoadingTests, OnInitialLoadGotMoreObjectsWithKey)
{
    auto const data = createTestData();
    auto const lastKey = std::make_optional<std::string>("something");

    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialObjects(kSEQ, data.objects, lastKey->data()));

    loader_.onInitialLoadGotMoreObjects(kSEQ, data.objects, lastKey);
}

TEST_F(LoadingTests, OnInitialLoadGotMoreObjectsWithoutKey)
{
    auto const data = createTestData();
    auto const lastKey = std::optional<std::string>{};

    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialObjects(kSEQ, data.objects, std::string{}));

    loader_.onInitialLoadGotMoreObjects(kSEQ, data.objects, lastKey);
}

TEST_F(LoadingTests, OnInitialLoadGotMoreObjectsFailure)
{
    auto const data = createTestData();
    auto const lastKey = std::optional<std::string>{};

    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialObjects(kSEQ, data.objects, std::string{}))
        .WillOnce([](auto, auto, auto) { throw std::runtime_error("some error"); });
    EXPECT_CALL(*mockAmendmentBlockHandlerPtr_, notifyAmendmentBlocked());

    loader_.onInitialLoadGotMoreObjects(kSEQ, data.objects, lastKey);
}

TEST_F(LoadingTests, LoadInitialLedgerFailure)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_))
        .WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*backend_, doFinishWrites()).Times(0);
    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialData(data)).WillOnce([](auto const&) {
        throw std::runtime_error("some error");
    });
    EXPECT_CALL(*mockAmendmentBlockHandlerPtr_, notifyAmendmentBlocked());

    auto const res = loader_.loadInitialLedger(data);
    EXPECT_FALSE(res.has_value());
}

TEST_F(LoadingAssertTest, LoadInitialLedgerHasDataInDB)
{
    auto const data = createTestData();
    auto const range = LedgerRange{.minSequence = kSEQ - 1, .maxSequence = kSEQ};

    // backend_ leaks due to death test. would be nice to figure out a better solution but for now
    // we simply don't set expectations and allow the mock to leak
    testing::Mock::AllowLeak(&*backend_);
    ON_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillByDefault(testing::Return(range));

    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = loader_.loadInitialLedger(data); });
}

TEST_F(LoadingTests, LoadWriteConflictEmitsStopWritingSignal)
{
    state_->isWriting = true;  // writer is active
    auto const data = createTestData();
    testing::StrictMock<testing::MockFunction<void(etl::SystemState::WriteCommand)>>
        mockSignalCallback;

    auto connection = state_->writeCommandSignal.connect(mockSignalCallback.AsStdFunction());

    EXPECT_CALL(*mockRegistryPtr_, dispatch(data));
    EXPECT_CALL(*backend_, doFinishWrites())
        .WillOnce(testing::Return(false));  // simulate write conflict
    EXPECT_CALL(mockSignalCallback, Call(etl::SystemState::WriteCommand::StopWriting));

    EXPECT_FALSE(state_->isWriterDecidingFallback);

    auto result = loader_.load(data);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), etl::LoaderError::WriteConflict);
    EXPECT_TRUE(state_->isWriterDecidingFallback);
}

TEST_F(LoadingTests, LoadSuccessDoesNotEmitSignal)
{
    state_->isWriting = true;  // writer is active
    auto const data = createTestData();
    testing::StrictMock<testing::MockFunction<void(etl::SystemState::WriteCommand)>>
        mockSignalCallback;

    auto connection = state_->writeCommandSignal.connect(mockSignalCallback.AsStdFunction());

    EXPECT_CALL(*mockRegistryPtr_, dispatch(data));
    EXPECT_CALL(*backend_, doFinishWrites()).WillOnce(testing::Return(true));  // success
    // No signal should be emitted on success

    EXPECT_FALSE(state_->isWriterDecidingFallback);

    auto result = loader_.load(data);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(state_->isWriterDecidingFallback);
}

TEST_F(LoadingTests, LoadWhenNotWritingDoesNotCheckConflict)
{
    state_->isWriting = false;  // not a writer
    auto const data = createTestData();
    testing::StrictMock<testing::MockFunction<void(etl::SystemState::WriteCommand)>>
        mockSignalCallback;

    auto connection = state_->writeCommandSignal.connect(mockSignalCallback.AsStdFunction());

    EXPECT_CALL(*mockRegistryPtr_, dispatch(data));
    // doFinishWrites should not be called when not writing
    EXPECT_CALL(*backend_, doFinishWrites()).Times(0);
    // No signal should be emitted

    auto result = loader_.load(data);
    EXPECT_TRUE(result.has_value());
}
