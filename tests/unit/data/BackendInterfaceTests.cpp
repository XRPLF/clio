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

#include "etl/CorruptionDetector.hpp"
#include "etl/SystemState.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/XRPAmount.h>

#include <optional>
#include <vector>

using namespace data;
using namespace util::prometheus;
using namespace testing;

namespace {

constexpr auto kMAX_SEQ = 30;
constexpr auto kMIN_SEQ = 10;

}  // namespace

struct BackendInterfaceTest : WithPrometheus, MockBackendTestNaggy, SyncAsioContextTest {
    BackendInterfaceTest()
    {
        backend_->setRange(kMIN_SEQ, kMAX_SEQ);
    }
};

TEST_F(BackendInterfaceTest, FetchFeesSuccessPath)
{
    using namespace ripple;

    // New fee setting (after XRPFees amendment)
    EXPECT_CALL(*backend_, doFetchLedgerObject(keylet::fees().key, kMAX_SEQ, _))
        .WillRepeatedly(Return(createFeeSettingBlob(XRPAmount(1), XRPAmount(2), XRPAmount(3), 0)));

    runSpawn([this](auto yield) {
        auto fees = backend_->fetchFees(kMAX_SEQ, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}

TEST_F(BackendInterfaceTest, FetchFeesLegacySuccessPath)
{
    using namespace ripple;

    // Legacy fee setting (before XRPFees amendment)
    EXPECT_CALL(*backend_, doFetchLedgerObject(keylet::fees().key, kMAX_SEQ, _))
        .WillRepeatedly(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    runSpawn([this](auto yield) {
        auto fees = backend_->fetchFees(kMAX_SEQ, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}

TEST_F(BackendInterfaceTest, FetchLedgerPageSuccessPath)
{
    using namespace ripple;

    auto state = etl::SystemState{};
    backend_->setCorruptionDetector(etl::CorruptionDetector{state, backend_->cache()});

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"}));
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _)).WillOnce(Return(std::vector<Blob>(10, Blob{'s'})));

    runSpawn([this](auto yield) { backend_->fetchLedgerPage(std::nullopt, kMAX_SEQ, 10, false, yield); });
    EXPECT_FALSE(backend_->cache().isDisabled());
}

TEST_F(BackendInterfaceTest, FetchLedgerPageDisablesCacheOnMissingData)
{
    using namespace ripple;

    auto state = etl::SystemState{};
    backend_->setCorruptionDetector(etl::CorruptionDetector{state, backend_->cache()});

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"}));
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _))
        .WillOnce(Return(std::vector<Blob>{
            Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{}
        }));

    runSpawn([this](auto yield) { backend_->fetchLedgerPage(std::nullopt, kMAX_SEQ, 10, false, yield); });
    EXPECT_TRUE(backend_->cache().isDisabled());
}

TEST_F(BackendInterfaceTest, FetchLedgerPageWithoutCorruptionDetectorDoesNotDisableCacheOnMissingData)
{
    using namespace ripple;

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"}));
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _))
        .WillOnce(Return(std::vector<Blob>{
            Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{'s'}, Blob{}
        }));

    runSpawn([this](auto yield) { backend_->fetchLedgerPage(std::nullopt, kMAX_SEQ, 10, false, yield); });
    EXPECT_FALSE(backend_->cache().isDisabled());
}
