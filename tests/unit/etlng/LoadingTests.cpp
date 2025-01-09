//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "data/Types.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/RegistryInterface.hpp"
#include "etlng/impl/Loading.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/BinaryTestObject.hpp"
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

using namespace etlng::model;
using namespace etlng::impl;

namespace {

constinit auto const kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kSEQ = 30;

struct MockRegistry : etlng::RegistryInterface {
    MOCK_METHOD(void, dispatchInitialObjects, (uint32_t, std::vector<Object> const&, std::string), (override));
    MOCK_METHOD(void, dispatchInitialData, (LedgerData const&), (override));
    MOCK_METHOD(void, dispatch, (LedgerData const&), (override));
};

struct MockLoadObserver : etlng::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<Object> const&, std::optional<std::string>),
        (override)
    );
};

struct LoadingTests : util::prometheus::WithPrometheus,
                      MockBackendTest,
                      MockLedgerFetcherTest,
                      MockAmendmentBlockHandlerTest {
protected:
    std::shared_ptr<MockRegistry> mockRegistryPtr_ = std::make_shared<MockRegistry>();
    Loader loader_{backend_, mockLedgerFetcherPtr_, mockRegistryPtr_, mockAmendmentBlockHandlerPtr_};
};

struct LoadingDeathTest : LoadingTests {};

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

    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*backend_, doFinishWrites());
    EXPECT_CALL(*mockRegistryPtr_, dispatchInitialData(data));

    auto const res = loader_.loadInitialLedger(data);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(rpc::ledgerHeaderToBlob(res.value(), true), rpc::ledgerHeaderToBlob(data.header, true));
}

TEST_F(LoadingTests, LoadSuccess)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, doFinishWrites());
    EXPECT_CALL(*mockRegistryPtr_, dispatch(data));

    loader_.load(data);
}

TEST_F(LoadingTests, LoadFailure)
{
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

TEST_F(LoadingDeathTest, LoadInitialLedgerHasDataInDB)
{
    auto const data = createTestData();
    auto const range = LedgerRange{.minSequence = kSEQ - 1, .maxSequence = kSEQ};

    // backend_ leaks due to death test. would be nice to figure out a better solution but for now
    // we simply don't set expectations and allow the mock to leak
    testing::Mock::AllowLeak(&*backend_);
    ON_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillByDefault(testing::Return(range));

    EXPECT_DEATH({ [[maybe_unused]] auto unused = loader_.loadInitialLedger(data); }, ".*");
}
