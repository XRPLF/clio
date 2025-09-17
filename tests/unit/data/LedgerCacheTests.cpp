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

#include "data/LedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/prometheus/Bool.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace data;

struct LedgerCacheTest : util::prometheus::WithPrometheus {
    LedgerCache cache;
};

TEST_F(LedgerCacheTest, defaultState)
{
    EXPECT_FALSE(cache.isDisabled());
    EXPECT_FALSE(cache.isFull());
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.latestLedgerSequence(), 0u);
}

struct LedgerCachePrometheusMetricTest : util::prometheus::WithMockPrometheus {
    LedgerCache cache;
};

TEST_F(LedgerCachePrometheusMetricTest, setDisabled)
{
    auto& disabledMock = makeMock<util::prometheus::Bool>("ledger_cache_disabled", {});

    EXPECT_CALL(disabledMock, set(1));
    cache.setDisabled();

    EXPECT_CALL(disabledMock, value()).WillOnce(testing::Return(1));
    EXPECT_TRUE(cache.isDisabled());
}

TEST_F(LedgerCachePrometheusMetricTest, setFull)
{
    auto& fullMock = makeMock<util::prometheus::Bool>("ledger_cache_full", {});
    auto& disabledMock = makeMock<util::prometheus::Bool>("ledger_cache_disabled", {});

    EXPECT_CALL(disabledMock, value()).WillOnce(testing::Return(0));
    EXPECT_CALL(fullMock, set(1));
    cache.setFull();

    EXPECT_CALL(fullMock, value()).WillOnce(testing::Return(1));
    EXPECT_TRUE(cache.isFull());
}
