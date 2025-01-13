//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/LoggerFixtures.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
using namespace util;

// Used as a fixture for tests with enabled logging
class LoggerTest : public LoggerFixture {};

// Used as a fixture for tests with disabled logging
class NoLoggerTest : public NoLoggerFixture {};

TEST_F(LoggerTest, Basic)
{
    Logger const log{"General"};
    log.info() << "Info line logged";
    checkEqual("General:NFO Info line logged");

    LogService::debug() << "Debug line with numbers " << 12345;
    checkEqual("General:DBG Debug line with numbers 12345");

    LogService::warn() << "Warning is logged";
    checkEqual("General:WRN Warning is logged");
}

TEST_F(LoggerTest, Filtering)
{
    Logger const log{"General"};
    log.trace() << "Should not be logged";
    checkEmpty();

    log.warn() << "Warning is logged";
    checkEqual("General:WRN Warning is logged");

    Logger const tlog{"Trace"};
    tlog.trace() << "Trace line logged for 'Trace' component";
    checkEqual("Trace:TRC Trace line logged for 'Trace' component");
}

using util::config::Array;
using util::config::ConfigFileJson;
using util::config::ConfigType;
using util::config::ConfigValue;

struct LoggerInitTest : LoggerTest {
protected:
    util::config::ClioConfigDefinition config_{
        {"log_channels.[].channel", Array{ConfigValue{ConfigType::String}.optional()}},
        {"log_channels.[].log_level", Array{ConfigValue{ConfigType::String}.optional()}},

        {"log_level", ConfigValue{ConfigType::String}.defaultValue("info")},

        {"log_format",
         ConfigValue{ConfigType::String}.defaultValue(
             R"(%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%)"
         )},

        {"log_to_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

        {"log_directory", ConfigValue{ConfigType::String}.optional()},

        {"log_rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048)},

        {"log_directory_max_size", ConfigValue{ConfigType::Integer}.defaultValue(50 * 1024)},

        {"log_rotation_hour_interval", ConfigValue{ConfigType::Integer}.defaultValue(12)},

        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
    };
};

TEST_F(LoggerInitTest, DefaultLogLevel)
{
    auto const parsingErrors = config_.parse(ConfigFileJson{boost::json::object{{"log_level", "warn"}}});
    ASSERT_FALSE(parsingErrors.has_value());
    std::string const logString = "some log";

    LogService::init(config_);
    for (auto const& channel : Logger::kCHANNELS) {
        Logger const log{channel};
        log.trace() << logString;
        checkEmpty();

        log.debug() << logString;
        checkEmpty();

        log.info() << logString;
        checkEmpty();

        log.warn() << logString;
        checkEqual(fmt::format("{}:WRN {}", channel, logString));

        log.error() << logString;
        checkEqual(fmt::format("{}:ERR {}", channel, logString));
    }
}

TEST_F(LoggerInitTest, ChannelLogLevel)
{
    std::string const configStr = R"json(
    {
        "log_level": "error",
        "log_channels": [
            {
                "channel": "Backend",
                "log_level": "warning"
            }
        ]
    }
    )json";

    auto const parsingErrors = config_.parse(ConfigFileJson{boost::json::parse(configStr).as_object()});
    ASSERT_FALSE(parsingErrors.has_value());
    std::string const logString = "some log";

    LogService::init(config_);
    for (auto const& channel : Logger::kCHANNELS) {
        Logger const log{channel};
        log.trace() << logString;
        checkEmpty();

        log.debug() << logString;
        checkEmpty();

        log.info() << logString;
        checkEmpty();

        log.warn() << logString;
        if (std::string_view{channel} == "Backend") {
            checkEqual(fmt::format("{}:WRN {}", channel, logString));
        } else {
            checkEmpty();
        }

        log.error() << "some log";
        checkEqual(fmt::format("{}:ERR {}", channel, logString));
    }
}

#ifndef COVERAGE_ENABLED
TEST_F(LoggerTest, LOGMacro)
{
    Logger const log{"General"};

    auto computeCalled = false;
    auto compute = [&computeCalled]() {
        computeCalled = true;
        return "computed";
    };

    LOG(log.trace()) << compute();
    EXPECT_FALSE(computeCalled);

    log.trace() << compute();
    EXPECT_TRUE(computeCalled);
}
#endif

TEST_F(NoLoggerTest, Basic)
{
    Logger const log{"Trace"};
    log.trace() << "Nothing";
    checkEmpty();

    LogService::fatal() << "Still nothing";
    checkEmpty();
}
