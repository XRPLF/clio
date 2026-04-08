#include "etl/CacheLoaderSettings.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

namespace json = boost::json;
using namespace etl;
using namespace testing;
using namespace util::config;

inline static ClioConfigDefinition
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
         {"cache.file.max_sequence_age", ConfigValue{ConfigType::Integer}.defaultValue(5000)}}
    };
}

inline static ClioConfigDefinition
getParseCacheConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = generateDefaultCacheConfig();
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
}

struct CacheLoaderSettingsTest : Test {};

TEST_F(CacheLoaderSettingsTest, DefaultSettingsParsedCorrectly)
{
    auto const cfg = generateDefaultCacheConfig();
    auto const settings = makeCacheLoaderSettings(cfg);
    auto const defaults = CacheLoaderSettings{};

    EXPECT_EQ(settings, defaults);
}

TEST_F(CacheLoaderSettingsTest, NumThreadsCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"io_threads": 42})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numThreads, 42);
}

TEST_F(CacheLoaderSettingsTest, NumDiffsCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"num_diffs": 42}})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheDiffs, 42);
}

TEST_F(CacheLoaderSettingsTest, NumMarkersCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"num_markers": 42}})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheMarkers, 42);
}

TEST_F(CacheLoaderSettingsTest, PageFetchSizeCorrectlyPropagatedThroughConfig)
{
    auto const cfg =
        getParseCacheConfig(json::parse(R"JSON({"cache": {"page_fetch_size": 42}})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.cachePageFetchSize, 42);
}

TEST_F(CacheLoaderSettingsTest, SyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "sYNC"}})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::SYNC);
    EXPECT_TRUE(settings.isSync());
}

TEST_F(CacheLoaderSettingsTest, AsyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "aSynC"}})JSON"));
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::ASYNC);
    EXPECT_TRUE(settings.isAsync());
}

TEST_F(CacheLoaderSettingsTest, NoLoadStyleCorrectlyPropagatedThroughConfig)
{
    {
        auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "nONe"}})JSON"));
        auto const settings = makeCacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NONE);
        EXPECT_TRUE(settings.isDisabled());
    }
    {
        auto const cfg = getParseCacheConfig(json::parse(R"JSON({"cache": {"load": "nO"}})JSON"));
        auto const settings = makeCacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NONE);
        EXPECT_TRUE(settings.isDisabled());
    }
}

TEST_F(CacheLoaderSettingsTest, CacheFilePathCorrectlyPropagatedThroughConfig)
{
    static constexpr auto kCACHE_FILE_PATH = "/path/to/cache.dat";
    auto const jsonStr =
        fmt::format(R"JSON({{"cache": {{"file": {{"path": "{}"}}}}}})JSON", kCACHE_FILE_PATH);
    auto const cfg = getParseCacheConfig(json::parse(jsonStr));
    auto const settings = makeCacheLoaderSettings(cfg);

    ASSERT_TRUE(settings.cacheFileSettings.has_value());
    EXPECT_EQ(settings.cacheFileSettings->path, kCACHE_FILE_PATH);
}

TEST_F(CacheLoaderSettingsTest, CacheFilePathNotSetWhenAbsentFromConfig)
{
    auto const cfg = generateDefaultCacheConfig();
    auto const settings = makeCacheLoaderSettings(cfg);

    EXPECT_FALSE(settings.cacheFileSettings.has_value());
}

TEST_F(CacheLoaderSettingsTest, MaxSequenceLagPropagatedThoughConfig)
{
    auto const seq = 1234;
    auto const jsonStr = fmt::format(
        R"JSON({{"cache": {{"file": {{"path": "doesnt_matter", "max_sequence_age": {} }}}}}})JSON",
        seq
    );
    auto const cfg = getParseCacheConfig(json::parse(jsonStr));
    auto const settings = makeCacheLoaderSettings(cfg);

    ASSERT_TRUE(settings.cacheFileSettings.has_value());
    EXPECT_EQ(settings.cacheFileSettings->maxAge, seq);
}
