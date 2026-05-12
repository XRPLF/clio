#include "data/LedgerCache.hpp"
#include "etl/Models.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TmpFile.hpp"
#include "util/prometheus/Bool.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace data;

struct LedgerCacheTest : util::prometheus::WithPrometheus {
    LedgerCache cache;
};

TEST_F(LedgerCacheTest, defaultState)
{
    EXPECT_FALSE(cache.isDisabled());
    EXPECT_FALSE(cache.isFull());
    EXPECT_FALSE(cache.isCurrentlyLoading());
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.latestLedgerSequence(), 0u);
}

TEST_F(LedgerCacheTest, startLoadingSetsIsCurrentlyLoading)
{
    EXPECT_FALSE(cache.isCurrentlyLoading());
    cache.startLoading();
    EXPECT_TRUE(cache.isCurrentlyLoading());
}

TEST_F(LedgerCacheTest, setFullResetsIsCurrentlyLoading)
{
    cache.startLoading();
    ASSERT_TRUE(cache.isCurrentlyLoading());
    cache.setFull();
    EXPECT_FALSE(cache.isCurrentlyLoading());
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
    auto& loadingMock = makeMock<util::prometheus::Bool>("ledger_cache_is_currently_loading", {});

    EXPECT_CALL(disabledMock, value()).WillOnce(testing::Return(0));
    EXPECT_CALL(fullMock, set(1));
    EXPECT_CALL(loadingMock, set(0));
    cache.setFull();

    EXPECT_CALL(fullMock, value()).WillOnce(testing::Return(1));
    EXPECT_TRUE(cache.isFull());
}

TEST_F(LedgerCachePrometheusMetricTest, startLoading)
{
    auto& loadingMock = makeMock<util::prometheus::Bool>("ledger_cache_is_currently_loading", {});

    EXPECT_CALL(loadingMock, set(1));
    cache.startLoading();

    EXPECT_CALL(loadingMock, value()).WillOnce(testing::Return(1));
    EXPECT_TRUE(cache.isCurrentlyLoading());
}

struct LedgerCacheSaveLoadTest : LedgerCacheTest {
    ripple::uint256 const key1{1};
    ripple::uint256 const key2{2};
    std::vector<etl::model::Object> const objs{
        etl::model::Object{
            .key = key1,
            .keyRaw = {},
            .data = {1, 2, 3, 4, 5},
            .dataRaw = {},
            .successor = {},
            .predecessor = {},
            .type = {}
        },
        etl::model::Object{
            .key = key2,
            .keyRaw = {},
            .data = {6, 7, 8, 9, 10},
            .dataRaw = {},
            .successor = {},
            .predecessor = {},
            .type = {}
        }
    };
    uint32_t const kLEDGER_SEQ = 100;
};

TEST_F(LedgerCacheSaveLoadTest, saveToFileFailsWhenCacheNotFull)
{
    auto const tmpFile = TmpFile::empty();
    ASSERT_FALSE(cache.isFull());
    auto const result = cache.saveToFile(tmpFile.path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Ledger cache is not full");
}

TEST_F(LedgerCacheSaveLoadTest, saveAndLoadFromFile)
{
    cache.update(objs, kLEDGER_SEQ);
    cache.setFull();

    ASSERT_TRUE(cache.isFull());
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.latestLedgerSequence(), kLEDGER_SEQ);

    auto const blob1 = cache.get(key1, kLEDGER_SEQ);
    ASSERT_TRUE(blob1.has_value());
    EXPECT_EQ(blob1.value(), objs.front().data);  // NOLINT(bugprone-unchecked-optional-access)

    auto const blob2 = cache.get(key2, kLEDGER_SEQ);
    ASSERT_TRUE(blob2.has_value());
    EXPECT_EQ(blob2.value(), objs.back().data);  // NOLINT(bugprone-unchecked-optional-access)

    auto const tmpFile = TmpFile::empty();
    auto const saveResult = cache.saveToFile(tmpFile.path);
    ASSERT_TRUE(saveResult.has_value()) << "Save failed: " << saveResult.error();

    LedgerCache newCache;
    auto const loadResult = newCache.loadFromFile(tmpFile.path, 0);
    ASSERT_TRUE(loadResult.has_value()) << "Load failed: " << loadResult.error();

    EXPECT_TRUE(newCache.isFull());
    EXPECT_EQ(newCache.size(), 2u);
    EXPECT_EQ(newCache.latestLedgerSequence(), kLEDGER_SEQ);

    auto const loadedBlob1 = newCache.get(key1, kLEDGER_SEQ);
    ASSERT_TRUE(loadedBlob1.has_value());
    EXPECT_EQ(loadedBlob1.value(), blob1);  // NOLINT(bugprone-unchecked-optional-access)

    auto const loadedBlob2 = newCache.get(key2, kLEDGER_SEQ);
    ASSERT_TRUE(loadedBlob2.has_value());
    EXPECT_EQ(loadedBlob2.value(), blob2);  // NOLINT(bugprone-unchecked-optional-access)

    EXPECT_EQ(newCache.latestLedgerSequence(), cache.latestLedgerSequence());
}

TEST_F(LedgerCacheSaveLoadTest, saveAndLoadFromFileWithDeletedObjects)
{
    cache.update(objs, kLEDGER_SEQ - 1);

    auto objsCopy = objs;
    objsCopy.front().data = {};

    cache.update(objsCopy, kLEDGER_SEQ);
    cache.setFull();

    // Verify deleted object is accessible via getDeleted
    auto const blob1 = cache.get(key1, kLEDGER_SEQ);
    ASSERT_FALSE(blob1.has_value());

    auto const blob2 = cache.get(key2, kLEDGER_SEQ);
    ASSERT_TRUE(blob2.has_value());
    EXPECT_EQ(blob2.value(), objs.back().data);  // NOLINT(bugprone-unchecked-optional-access)

    auto const deletedBlob = cache.getDeleted(key1, kLEDGER_SEQ - 1);
    ASSERT_TRUE(deletedBlob.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(deletedBlob.value(), objs.front().data);

    // Save and load
    auto const tmpFile = TmpFile::empty();
    auto saveResult = cache.saveToFile(tmpFile.path);
    ASSERT_TRUE(saveResult.has_value()) << "Save failed: " << saveResult.error();

    LedgerCache newCache;
    auto loadResult = newCache.loadFromFile(tmpFile.path, 0);
    ASSERT_TRUE(loadResult.has_value()) << "Load failed: " << loadResult.error();

    // Verify deleted object is preserved
    auto const loadedDeletedBlob = newCache.getDeleted(key1, kLEDGER_SEQ - 1);
    ASSERT_TRUE(loadedDeletedBlob.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(loadedDeletedBlob.value(), deletedBlob);

    // Verify active object
    auto const loadedBlob1 = newCache.get(key1, kLEDGER_SEQ);
    ASSERT_FALSE(loadedBlob1.has_value());

    auto const loadedBlob2 = newCache.get(key2, kLEDGER_SEQ);
    ASSERT_TRUE(loadedBlob2.has_value());
    EXPECT_EQ(loadedBlob2.value(), blob2);  // NOLINT(bugprone-unchecked-optional-access)

    EXPECT_TRUE(newCache.isFull());
    EXPECT_EQ(newCache.latestLedgerSequence(), cache.latestLedgerSequence());
}

TEST_F(LedgerCacheTest, SaveFailedDueToFilePermissions)
{
    cache.setFull();
    auto const result = cache.saveToFile("/");
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(LedgerCacheTest, loadFromNonExistentFileReturnsError)
{
    auto const result = cache.loadFromFile("/nonexistent/path/cache.dat", 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(LedgerCacheSaveLoadTest, RejectOldCacheFile)
{
    uint32_t const cacheSeq = 100;
    cache.update(objs, cacheSeq);
    cache.setFull();

    auto const tmpFile = TmpFile::empty();
    auto const saveResult = cache.saveToFile(tmpFile.path);
    ASSERT_TRUE(saveResult.has_value());

    LedgerCache newCache;
    auto const loadResult = newCache.loadFromFile(tmpFile.path, cacheSeq + 1);
    EXPECT_FALSE(loadResult.has_value());
    EXPECT_THAT(loadResult.error(), ::testing::HasSubstr("too low"));
}

TEST_F(LedgerCacheSaveLoadTest, AcceptRecentCacheFile)
{
    uint32_t const cacheSeq = 100;
    cache.update(objs, cacheSeq);
    cache.setFull();

    auto const tmpFile = TmpFile::empty();
    auto const saveResult = cache.saveToFile(tmpFile.path);
    ASSERT_TRUE(saveResult.has_value());

    LedgerCache newCache;
    auto const loadResult = newCache.loadFromFile(tmpFile.path, cacheSeq - 1);
    ASSERT_TRUE(loadResult.has_value());
    EXPECT_EQ(newCache.latestLedgerSequence(), cacheSeq);
}
