#include "data/LedgerHeaderCache.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>

using namespace data;
using Test = ::testing::Test;

constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kLedgerHasH2 =
    "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

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
    auto const ledger = createLedgerHeader(kLedgerHash, 42);
    FetchLedgerCache::CacheEntry const entry{.ledger = ledger, .seq = 42};

    cache_.put(entry);
    auto const result = cache_.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_F(FetchLedgerCacheTest, PutOverwritesPreviousEntry)
{
    auto const ledger1 = createLedgerHeader(kLedgerHash, 1);
    auto const ledger2 = createLedgerHeader(kLedgerHasH2, 2);

    FetchLedgerCache::CacheEntry const entry1{.ledger = ledger1, .seq = 1};
    FetchLedgerCache::CacheEntry const entry2{.ledger = ledger2, .seq = 2};

    cache_.put(entry1);
    cache_.put(entry2);

    auto const result = cache_.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), entry2);  // NOLINT(bugprone-unchecked-optional-access)
}
