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
#include "util/config/Config.hpp"

#include <boost/json.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace json = boost::json;
using namespace etl;
using namespace testing;

struct CacheLoaderSettingsTest : Test {};

TEST_F(CacheLoaderSettingsTest, DefaultSettingsParsedCorrectly)
{
    auto cfg = util::Config{json::parse(R"({})")};
    auto settings = make_CacheLoaderSettings(cfg);
    auto defaults = CacheLoaderSettings{};

    EXPECT_EQ(settings, defaults);
}

TEST_F(CacheLoaderSettingsTest, NumThreadsCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"io_threads": 42})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numThreads, 42);
}

TEST_F(CacheLoaderSettingsTest, NumDiffsCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"cache": {"num_diffs": 42}})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheDiffs, 42);
}

TEST_F(CacheLoaderSettingsTest, NumMarkersCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"cache": {"num_markers": 42}})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.numCacheMarkers, 42);
}

TEST_F(CacheLoaderSettingsTest, PageFetchSizeCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"cache": {"page_fetch_size": 42}})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.cachePageFetchSize, 42);
}

TEST_F(CacheLoaderSettingsTest, SyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"cache": {"load": "sYNC"}})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::SYNC);
    EXPECT_TRUE(settings.isSync());
}

TEST_F(CacheLoaderSettingsTest, AsyncLoadStyleCorrectlyPropagatedThroughConfig)
{
    auto cfg = util::Config{json::parse(R"({"cache": {"load": "aSynC"}})")};
    auto settings = make_CacheLoaderSettings(cfg);

    EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::ASYNC);
    EXPECT_TRUE(settings.isAsync());
}

TEST_F(CacheLoaderSettingsTest, NoLoadStyleCorrectlyPropagatedThroughConfig)
{
    {
        auto cfg = util::Config{json::parse(R"({"cache": {"load": "nONe"}})")};
        auto settings = make_CacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NOT_AT_ALL);
        EXPECT_TRUE(settings.isDisabled());
    }
    {
        auto cfg = util::Config{json::parse(R"({"cache": {"load": "nO"}})")};
        auto settings = make_CacheLoaderSettings(cfg);

        EXPECT_EQ(settings.loadStyle, CacheLoaderSettings::LoadStyle::NOT_AT_ALL);
        EXPECT_TRUE(settings.isDisabled());
    }
}
