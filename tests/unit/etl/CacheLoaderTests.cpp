//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include "etl/CacheLoader.hpp"
#include "etl/CacheLoaderSettings.hpp"
#include "etl/FakeDiffProvider.hpp"
#include "etl/impl/CacheLoader.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace json = boost::json;
using namespace etl;
using namespace util;
using namespace data;
using namespace testing;
using namespace util::config;

namespace {

inline ClioConfigDefinition
generateDefaultCacheConfig()
{
    return ClioConfigDefinition{
        {{"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
         {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
         {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
         {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")},
         {"cache.file.path", ConfigValue{ConfigType::String}.optional()},
         {"cache.file.max_sequence_age", ConfigValue{ConfigType::Integer}.defaultValue(10)}}
    };
}

inline ClioConfigDefinition
getParseCacheConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = generateDefaultCacheConfig();
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
}

constexpr auto kSEQ = 30;

struct CacheLoaderTest : util::prometheus::WithPrometheus, MockBackendTest {
    DiffProvider diffProvider;
    MockLedgerCache cache;
};

using Settings = etl::CacheLoaderSettings;
struct ParametrizedCacheLoaderTest : CacheLoaderTest, WithParamInterface<Settings> {};

};  // namespace

//
// Tests of implementation
//
INSTANTIATE_TEST_CASE_P(
    CacheLoaderTest,
    ParametrizedCacheLoaderTest,
    Values(
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 512,
            .numThreads = 2,
            .cacheFileSettings = std::nullopt,
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 512,
            .numThreads = 4,
            .cacheFileSettings = std::nullopt,
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 512,
            .numThreads = 8,
            .cacheFileSettings = std::nullopt,
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 512,
            .numThreads = 16,
            .cacheFileSettings = std::nullopt,
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 128,
            .cachePageFetchSize = 24,
            .numThreads = 2,
            .cacheFileSettings = std::nullopt,
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 64,
            .cachePageFetchSize = 48,
            .numThreads = 4,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 64,
            .numThreads = 8,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 32,
            .numCacheMarkers = 24,
            .cachePageFetchSize = 128,
            .numThreads = 16,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 128,
            .numCacheMarkers = 128,
            .cachePageFetchSize = 24,
            .numThreads = 2,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 1024,
            .numCacheMarkers = 64,
            .cachePageFetchSize = 48,
            .numThreads = 4,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 512,
            .numCacheMarkers = 48,
            .cachePageFetchSize = 64,
            .numThreads = 8,
            .cacheFileSettings = std::nullopt
        },
        Settings{
            .numCacheDiffs = 64,
            .numCacheMarkers = 24,
            .cachePageFetchSize = 128,
            .numThreads = 16,
            .cacheFileSettings = std::nullopt
        }
    ),
    [](auto const& info) {
        auto const settings = info.param;
        return fmt::format(
            "diffs_{}__markers_{}__fetchSize_{}__threads_{}",
            settings.numCacheDiffs,
            settings.numCacheMarkers,
            settings.cachePageFetchSize,
            settings.numThreads
        );
    }
);

TEST_P(ParametrizedCacheLoaderTest, LoadCacheWithDifferentSettings)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _))
        .Times(keysSize * loops)
        .WillRepeatedly([this]() { return diffProvider.nextKey(keysSize); });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImpl).Times(loops);
    EXPECT_CALL(cache, setFull).Times(1);

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockLedgerCache> loader{
        ctx,
        backend_,
        cache,
        kSEQ,
        settings.numCacheMarkers,
        settings.cachePageFetchSize,
        provider.getCursors(kSEQ)
    };

    loader.wait();
}

TEST_P(ParametrizedCacheLoaderTest, AutomaticallyCancelledAndAwaitedInDestructor)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 1024;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _))
        .Times(AtMost(keysSize * loops))
        .WillRepeatedly([this]() { return diffProvider.nextKey(keysSize); });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImpl).Times(AtMost(loops));
    EXPECT_CALL(cache, setFull).Times(AtMost(1));

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockLedgerCache> const loader{
        ctx,
        backend_,
        cache,
        kSEQ,
        settings.numCacheMarkers,
        settings.cachePageFetchSize,
        provider.getCursors(kSEQ)
    };

    // no loader.wait(): loader is immediately stopped and awaited in destructor
}

TEST_P(ParametrizedCacheLoaderTest, CacheDisabledLeadsToCancellation)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 1024;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _))
        .Times(AtMost(keysSize * loops))
        .WillRepeatedly([this]() { return diffProvider.nextKey(keysSize); });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(cache, updateImpl).Times(AtMost(1));
    EXPECT_CALL(cache, setFull).Times(0);

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockLedgerCache> loader{
        ctx,
        backend_,
        cache,
        kSEQ,
        settings.numCacheMarkers,
        settings.cachePageFetchSize,
        provider.getCursors(kSEQ)
    };

    loader.wait();
}

//
// Tests of public CacheLoader interface
//
TEST_F(CacheLoaderTest, SyncCacheLoaderWaitsTillFullyLoaded)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "sync"}})JSON"));
    CacheLoader<> loader{cfg, backend_, cache};

    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(32).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(keysSize * loops).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .Times(loops)
        .WillRepeatedly(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImpl).Times(loops);
    EXPECT_CALL(cache, isFull).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(cache, setFull).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderTest, AsyncCacheLoaderCanBeStopped)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "async"}})JSON"));
    CacheLoader loader{cfg, backend_, cache};

    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(AtMost(32)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey)
        .Times(AtMost(keysSize * loops))
        .WillRepeatedly([this]() { return diffProvider.nextKey(keysSize); });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .Times(AtMost(loops))
        .WillRepeatedly(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImpl).Times(AtMost(loops));
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setFull).Times(AtMost(1));

    loader.load(kSEQ);
    loader.stop();
    loader.wait();
}

TEST_F(CacheLoaderTest, DisabledCacheLoaderDoesNotLoadCache)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "none"}})JSON"));
    CacheLoader loader{cfg, backend_, cache};

    EXPECT_CALL(cache, updateImpl).Times(0);
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setDisabled).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderTest, DisabledCacheLoaderCanCallStopAndWait)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "none"}})JSON"));
    CacheLoader loader{cfg, backend_, cache};

    EXPECT_CALL(cache, updateImpl).Times(0);
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setDisabled).Times(1);

    loader.load(kSEQ);

    EXPECT_NO_THROW(loader.stop());
    EXPECT_NO_THROW(loader.wait());
}

struct CacheLoaderFromFileTest : CacheLoaderTest {
    CacheLoaderFromFileTest()
    {
        backend_->setRange(kSEQ - 20, kSEQ);
    }

    std::string const filePath = "./cache.bin";
    uint32_t const maxSequenceLag = 10;
    ClioConfigDefinition const cfg = getParseCacheConfig(
        json::parse(
            fmt::format(
                R"JSON({{"cache": {{"load": "sync", "file": {{"path": "{}", "max_sequence_age": {}}}}}}})JSON",
                filePath,
                maxSequenceLag
            )
        )
    );
    CacheLoader<> loader{cfg, backend_, cache};
};

TEST_F(CacheLoaderFromFileTest, Success)
{
    constexpr uint32_t kLOADED_SEQ = 12345;

    EXPECT_CALL(cache, isFull).WillOnce(Return(false));
    EXPECT_CALL(cache, loadFromFile(filePath, kSEQ - maxSequenceLag))
        .WillOnce(Return(std::expected<void, std::string>{}));
    EXPECT_CALL(cache, latestLedgerSequence).WillOnce(Return(kLOADED_SEQ));
    EXPECT_CALL(cache, setFull);

    loader.load(kSEQ);

    std::optional<LedgerRange> const expectedLedgerRange =
        LedgerRange{.minSequence = kSEQ - 20, .maxSequence = kSEQ};
    EXPECT_EQ(backend_->fetchLedgerRange(), expectedLedgerRange);
}

TEST_F(CacheLoaderFromFileTest, FailureBackToNormalLoad)
{
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(cache, loadFromFile(filePath, kSEQ - maxSequenceLag))
        .WillOnce(Return(std::expected<void, std::string>(std::unexpected("File not found"))));

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(32).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(keysSize * loops).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .Times(loops)
        .WillRepeatedly(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImpl).Times(loops);
    EXPECT_CALL(cache, isFull).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(cache, setFull).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderFromFileTest, DontLoadWhenCacheIsDisabled)
{
    auto const disabledCacheCfg = getParseCacheConfig(
        json::parse(R"JSON({"cache": {"load": "none", "file": {"path": "/tmp/cache.bin"}}})JSON")
    );
    CacheLoader loaderWithCacheDisabled{disabledCacheCfg, backend_, cache};

    EXPECT_CALL(cache, isFull).WillOnce(Return(false));
    EXPECT_CALL(cache, setDisabled);

    loaderWithCacheDisabled.load(kSEQ);
}

TEST_F(CacheLoaderFromFileTest, MaxSequenceLagCalculation)
{
    constexpr uint32_t kLOADED_SEQ = 12345;

    EXPECT_CALL(cache, isFull).WillOnce(Return(false));
    EXPECT_CALL(cache, loadFromFile(filePath, kSEQ - maxSequenceLag))
        .WillOnce(Return(std::expected<void, std::string>{}));
    EXPECT_CALL(cache, latestLedgerSequence).WillOnce(Return(kLOADED_SEQ));

    loader.load(kSEQ);
}

TEST_F(CacheLoaderFromFileTest, FileSequenceBehindBackendFetchesMissingLedgersFromDB)
{
    constexpr uint32_t kFILE_SEQ = kSEQ - 2;
    auto const diffs = diffProvider.getLatestDiff();

    EXPECT_CALL(cache, isFull).WillOnce(Return(false));
    EXPECT_CALL(cache, loadFromFile(filePath, kSEQ - maxSequenceLag))
        .WillOnce(Return(std::expected<void, std::string>{}));

    // latestLedgerSequence is called twice per loop iteration (condition + seqToLoad + 1)
    // plus once for the final exit check
    EXPECT_CALL(cache, latestLedgerSequence)
        .WillOnce(Return(kFILE_SEQ))      // iteration 1: condition (true)
        .WillOnce(Return(kFILE_SEQ))      // iteration 1: seqToLoad + 1 = kFILE_SEQ + 1
        .WillOnce(Return(kFILE_SEQ + 1))  // iteration 2: condition (true)
        .WillOnce(Return(kFILE_SEQ + 1))  // iteration 2: seqToLoad + 1 = kFILE_SEQ + 2
        .WillOnce(Return(kSEQ));          // exit condition (false)

    EXPECT_CALL(*backend_, fetchLedgerDiff(kFILE_SEQ + 1, _)).WillOnce(Return(diffs));
    EXPECT_CALL(*backend_, fetchLedgerDiff(kFILE_SEQ + 2, _)).WillOnce(Return(diffs));
    EXPECT_CALL(cache, updateImpl).Times(2);
    EXPECT_CALL(cache, setFull).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderFromFileTest, MaxSequenceLagClampedToMinOfLedgerRange)
{
    uint32_t const currentSeq = 110;
    uint32_t const minSeq = currentSeq - maxSequenceLag + 10;
    backend_->setRange(minSeq, currentSeq, true);

    EXPECT_CALL(cache, isFull).WillOnce(Return(false));
    EXPECT_CALL(cache, loadFromFile(filePath, minSeq))
        .WillOnce(Return(std::expected<void, std::string>{}));
    EXPECT_CALL(cache, latestLedgerSequence).WillOnce(Return(minSeq + 1));

    loader.load(currentSeq);
}
