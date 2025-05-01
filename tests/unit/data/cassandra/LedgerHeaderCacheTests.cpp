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

#include "data/LedgerHeaderCache.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

using namespace data;
using Test = ::testing::Test;

constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kLEDGER_HASH2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

class FetchLedgerCacheTest : public Test {
protected:
    FetchLedgerCache cache_;
};

TEST_F(FetchLedgerCacheTest, DefaultCacheIsEmpty)
{
    auto const result = cache_.get();
    EXPECT_FALSE(result.has_value());
}

TEST_F(FetchLedgerCacheTest, CanStoreAndRetrieveEntry)
{
    auto const ledger = createLedgerHeader(kLEDGER_HASH, 42);
    FetchLedgerCache::CacheEntry entry{.ledger = ledger, .seq = 42};

    cache_.put(entry);
    auto const result = cache_.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry);
}

TEST_F(FetchLedgerCacheTest, PutOverwritesPreviousEntry)
{
    auto const ledger1 = createLedgerHeader(kLEDGER_HASH, 1);
    auto const ledger2 = createLedgerHeader(kLEDGER_HASH2, 2);

    FetchLedgerCache::CacheEntry entry1{.ledger = ledger1, .seq = 1};
    FetchLedgerCache::CacheEntry entry2{.ledger = ledger2, .seq = 2};

    cache_.put(entry1);
    cache_.put(entry2);

    auto const result = cache_.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry2);
}
