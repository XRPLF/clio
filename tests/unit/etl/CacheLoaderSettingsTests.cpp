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

#include "etl/CacheLoaderSettings.hpp"
#include "util/Assert.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

namespace json = boost::json;
using namespace etl;
using namespace testing;
using namespace util::config;

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
         {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")}}
    };
}

inline ClioConfigDefinition
getParseCacheConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = generateDefaultCacheConfig();
    auto const errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Error parsing Json for clio config for settings test");
    return config;
}

struct CacheLoaderSettingsTest : Test {};

TEST_F(CacheLoaderSettingsTest, DefaultSettingsParsedCorrectly)
{
    auto const cfg = generateDefaultCacheConfig();
    auto const settings = make_CacheLoaderSettings(cfg);
    auto const defaults = CacheLoaderSettings{};

    EXPECT_EQ(settings, defaults);
}

TEST_F(CacheLoaderSettingsTest, NumThreadsCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"io_threads": 42})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numThreads, 42);
}

TEST_F(CacheLoaderSettingsTest, NumDiffsCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"num_diffs": 42}})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheDiffs, 42);
}

TEST_F(CacheLoaderSettingsTest, NumMarkersCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"num_markers": 42}})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheMarkers, 42);
}

TEST_F(CacheLoaderSettingsTest, PageFetchSizeCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"page_fetch_size": 42}})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.cachePageFetchSize, 42);
}

TEST_F(CacheLoaderSettingsTest, SyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "sYNC"}})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::SYNC);
    EXPECT_TRUE(settings.isSync());
}

TEST_F(CacheLoaderSettingsTest, AsyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "aSynC"}})"));
    auto const settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::ASYNC);
    EXPECT_TRUE(settings.isAsync());
}

TEST_F(CacheLoaderSettingsTest, NoLoadStyleCorrectlyPropagatedThroughConfig)
{
    {
        auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "nONe"}})"));
        auto const settings = make_CacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NONE);
        EXPECT_TRUE(settings.isDisabled());
    }
    {
        auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "nO"}})"));
        auto const settings = make_CacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NONE);
        EXPECT_TRUE(settings.isDisabled());
    }
}
