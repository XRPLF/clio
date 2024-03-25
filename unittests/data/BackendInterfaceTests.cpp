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

#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/XRPAmount.h>
#include <ripple/protocol/Indexes.h>

using namespace data;
using namespace util::prometheus;
using namespace testing;

constexpr static auto MAXSEQ = 30;
constexpr static auto MINSEQ = 10;

struct BackendInterfaceTest : MockBackendTestNaggy, SyncAsioContextTest, WithPrometheus {};

TEST_F(BackendInterfaceTest, FetchFeesSuccessPath)
{
    using namespace ripple;
    backend->setRange(MINSEQ, MAXSEQ);

    // New fee setting (after XRPFees amendment)
    EXPECT_CALL(*backend, doFetchLedgerObject(keylet::fees().key, MAXSEQ, _))
        .WillRepeatedly(Return(CreateFeeSettingBlob(XRPAmount(1), XRPAmount(2), XRPAmount(3), 0)));

    runSpawn([this](auto yield) {
        auto fees = backend->fetchFees(MAXSEQ, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}

TEST_F(BackendInterfaceTest, FetchFeesLegacySuccessPath)
{
    using namespace ripple;
    backend->setRange(MINSEQ, MAXSEQ);

    // Legacy fee setting (before XRPFees amendment)
    EXPECT_CALL(*backend, doFetchLedgerObject(keylet::fees().key, MAXSEQ, _))
        .WillRepeatedly(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    runSpawn([this](auto yield) {
        auto fees = backend->fetchFees(MAXSEQ, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}
