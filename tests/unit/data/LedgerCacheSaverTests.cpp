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

#include "data/LedgerCacheSaver.hpp"
#include "util/MockAssert.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <expected>
#include <semaphore>
#include <string>
#include <thread>

using namespace data;
using namespace util::config;

struct LedgerCacheSaverTest : virtual testing::Test {
    testing::StrictMock<MockLedgerCache> cache;
    constexpr static auto kFILE_PATH = "./cache.bin";

    static ClioConfigDefinition
    generateConfig(bool cacheFilePathHasValue, bool asyncSave)
    {
        auto config = ClioConfigDefinition{{
            {"cache.file.path", ConfigValue{ConfigType::String}.optional()},
            {"cache.file.async_save", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
        }};

        ConfigFileJson jsonFile{boost::json::object{}};
        if (cacheFilePathHasValue) {
            auto const jsonObject = boost::json::parse(
                                        fmt::format(
                                            R"JSON({{"cache": {{"file": {{"path": "{}", "async_save": {} }} }} }})JSON",
                                            kFILE_PATH,
                                            asyncSave
                                        )
            )
                                        .as_object();
            jsonFile = ConfigFileJson{jsonObject};
        }
        auto const errors = config.parse(jsonFile);
        EXPECT_FALSE(errors.has_value());
        return config;
    }
};

TEST_F(LedgerCacheSaverTest, SaveSuccessfully)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);
    LedgerCacheSaver saver{config, cache};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH)).WillOnce(testing::Return(std::expected<void, std::string>{}));

    saver.save();
    saver.waitToFinish();
}

TEST_F(LedgerCacheSaverTest, SaveWithError)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);
    LedgerCacheSaver saver{config, cache};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH))
        .WillOnce(testing::Return(std::expected<void, std::string>(std::unexpected("Failed to save"))));

    saver.save();
    saver.waitToFinish();
}

TEST_F(LedgerCacheSaverTest, NoSaveWhenPathNotConfigured)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ false, /* asyncSave = */ true);

    LedgerCacheSaver saver{config, cache};
    saver.save();
    saver.waitToFinish();
}

TEST_F(LedgerCacheSaverTest, DestructorWaitsForCompletion)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);

    std::binary_semaphore semaphore{1};
    std::atomic_bool saveCompleted{false};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH)).WillOnce([&]() {
        semaphore.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        saveCompleted = true;
        return std::expected<void, std::string>{};
    });

    {
        LedgerCacheSaver saver{config, cache};
        saver.save();
        EXPECT_TRUE(semaphore.try_acquire_for(std::chrono::seconds{5}));
    }

    EXPECT_TRUE(saveCompleted);
}

TEST_F(LedgerCacheSaverTest, WaitToFinishCanBeCalledMultipleTimes)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);
    LedgerCacheSaver saver{config, cache};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH));

    saver.save();
    saver.waitToFinish();
    EXPECT_NO_THROW(saver.waitToFinish());
}

TEST_F(LedgerCacheSaverTest, WaitToFinishWithoutSaveIsSafe)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);
    LedgerCacheSaver saver{config, cache};
    EXPECT_NO_THROW(saver.waitToFinish());
}

struct LedgerCacheSaverAssertTest : LedgerCacheSaverTest, common::util::WithMockAssert {};

TEST_F(LedgerCacheSaverAssertTest, MultipleSavesNotAllowed)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);

    LedgerCacheSaver saver{config, cache};
    std::binary_semaphore semaphore{0};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH)).WillOnce([&](auto&&) {
        semaphore.acquire();
        return std::expected<void, std::string>{};
    });
    saver.save();
    EXPECT_CLIO_ASSERT_FAIL({ saver.save(); });
    semaphore.release();

    saver.waitToFinish();
}

TEST_F(LedgerCacheSaverTest, SyncSaveWaitsForCompletion)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ false);

    std::atomic_bool saveCompleted{false};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH)).WillOnce([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        saveCompleted = true;
        return std::expected<void, std::string>{};
    });

    LedgerCacheSaver saver{config, cache};
    saver.save();
    EXPECT_TRUE(saveCompleted);
}

TEST_F(LedgerCacheSaverTest, AsyncSaveDoesNotWaitForCompletion)
{
    auto const config = generateConfig(/* cacheFilePathHasValue = */ true, /* asyncSave = */ true);

    std::binary_semaphore saveStarted{0};
    std::binary_semaphore continueExecution{0};
    std::atomic_bool saveCompleted{false};

    EXPECT_CALL(cache, saveToFile(kFILE_PATH)).WillOnce([&]() {
        saveStarted.release();
        continueExecution.acquire();
        saveCompleted = true;
        return std::expected<void, std::string>{};
    });

    LedgerCacheSaver saver{config, cache};
    saver.save();

    EXPECT_TRUE(saveStarted.try_acquire_for(std::chrono::seconds{5}));
    EXPECT_FALSE(saveCompleted);

    continueExecution.release();
    saver.waitToFinish();
    EXPECT_TRUE(saveCompleted);
}
