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

#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <string>

using namespace data::cassandra;
using Test = ::testing::Test;

class FetchLedgerCacheTest : Test {
protected:
    FetchLedgerCache cache_;

    static ripple::LedgerHeader
    makeLedger(std::string const& hash, uint32_t const seq)
    {
        ripple::LedgerHeader header;
        header.hash = ripple::uint256{hash};
        header.seq = seq;
        return header;
    }
};

TEST_F(FetchLedgerCacheTest, DefaultCacheIsEmpty)
{
    auto result = cache_.get();
    EXPECT_FALSE(result.has_value());
}

TEST_F(FetchLedgerCacheTest, CanStoreAndRetrieveEntry)
{
    auto ledger = makeLedger("anything", 42);
    FetchLedgerCache::CacheEntry entry{.ledger = ledger, .seq = 42};

    cache_.put(entry);
    auto result = cache_.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry);
}

TEST_F(FetchLedgerCacheTest, PutOverwritesPreviousEntry)
{
    auto ledger1 = makeLedger("1234", 1);
    auto ledger2 = makeLedger("abcd", 2);

    FetchLedgerCache::CacheEntry entry1{.ledger = ledger1, .seq = 1};
    FetchLedgerCache::CacheEntry entry2{.ledger = ledger2, .seq = 2};

    cache_.put(entry1);
    cache_.put(entry2);

    auto result = cache_.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry2);
}
