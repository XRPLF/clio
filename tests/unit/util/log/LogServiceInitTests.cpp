#include "util/LoggerBuffer.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
using namespace util;

using util::config::Array;
using util::config::ConfigFileJson;
using util::config::ConfigType;
using util::config::ConfigValue;

struct LogServiceInitTests : virtual public LoggerFixture {
public:
    LogServiceInitTests()
    {
        LogServiceState::reset();
    }

    ~LogServiceInitTests() override
    {
        if (LogService::initialized()) {
            LogService::reset();
        }
        LogServiceState::init(false, Severity::FTL, {});
    }

protected:
    util::config::ClioConfigDefinition config_{
        {"log.channels.[].channel", Array{ConfigValue{ConfigType::String}}},
        {"log.channels.[].level", Array{ConfigValue{ConfigType::String}}},

        {"log.level", ConfigValue{ConfigType::String}.defaultValue("info")},

        {"log.format",
         ConfigValue{ConfigType::String}.defaultValue(R"(%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v)")},
        {"log.is_async", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

        {"log.enable_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

        {"log.directory", ConfigValue{ConfigType::String}.optional()},

        {"log.rotation_size",
         ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(
             config::gValidateUint32
         )},

        {"log.directory_max_files",
         ConfigValue{ConfigType::Integer}.defaultValue(25).withConstraint(config::gValidateUint32)},

        {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
    };

    std::string
    getLoggerString()
    {
        return buffer_.getStrAndReset();
    }
};

TEST_F(LogServiceInitTests, DefaultLogLevel)
{
    auto const parsingErrors = config_.parse(
        ConfigFileJson{boost::json::object{{"log", boost::json::object{{"level", "warn"}}}}}
    );
    ASSERT_FALSE(parsingErrors.has_value());

    EXPECT_TRUE(LogService::init(config_));

    std::string const logString = "some log";
    for (std::string_view const channel : Logger::kCHANNELS) {
        Logger const log{channel};
        log.trace() << logString;
        auto loggerStr = getLoggerString();
        ASSERT_TRUE(loggerStr.empty()) << " channel: " << channel << " logger_str: " << loggerStr;

        log.debug() << logString;
        ASSERT_TRUE(getLoggerString().empty());

        log.info() << logString;
        ASSERT_TRUE(getLoggerString().empty());

        log.warn() << logString;
        ASSERT_EQ(fmt::format("war:{} - {}\n", channel, logString), getLoggerString());

        log.error() << logString;
        ASSERT_EQ(fmt::format("err:{} - {}\n", channel, logString), getLoggerString());
    }
}

TEST_F(LogServiceInitTests, ChannelLogLevel)
{
    std::string const configStr = R"JSON(
        {
            "log": {
                "level": "error",
                "channels": [
                    {
                        "channel": "Backend",
                        "level": "warning"
                    }
                ]
            }
        }
    )JSON";

    auto const parsingErrors =
        config_.parse(ConfigFileJson{boost::json::parse(configStr).as_object()});
    ASSERT_FALSE(parsingErrors.has_value());

    EXPECT_TRUE(LogService::init(config_));

    std::string const logString = "some log";
    for (auto const& channel : Logger::kCHANNELS) {
        Logger const log{channel};
        log.trace() << logString;
        ASSERT_TRUE(getLoggerString().empty());

        log.debug() << logString;
        ASSERT_TRUE(getLoggerString().empty());

        log.info() << logString;
        ASSERT_TRUE(getLoggerString().empty());

        log.warn() << logString;
        if (std::string_view{channel} == "Backend") {
            ASSERT_EQ(fmt::format("war:{} - {}\n", channel, logString), getLoggerString());
        } else {
            ASSERT_TRUE(getLoggerString().empty());
        }

        log.error() << logString;
        ASSERT_EQ(fmt::format("err:{} - {}\n", channel, logString), getLoggerString());
    }
}

TEST_F(LogServiceInitTests, InitReturnsErrorIfCouldNotCreateLogDirectory)
{
    // "/proc" directory is read only on any unix OS
    auto const parsingErrors = config_.parse(
        ConfigFileJson{
            boost::json::object{{"log", boost::json::object{{"directory", "/proc/logs"}}}}
        }
    );
    ASSERT_FALSE(parsingErrors.has_value());

    auto const result = LogService::init(config_);
    EXPECT_FALSE(result);
    EXPECT_THAT(result.error(), testing::HasSubstr("Couldn't create logs directory"));
}

TEST_F(LogServiceInitTests, InitReturnsErrorIfProvidedInvalidChannel)
{
    auto const jsonStr = R"JSON(
        {
            "log": {
                "channels": [
                    {
                        "channel": "SomeChannel",
                        "level": "warn"
                    }
                ]
            }
    })JSON";

    auto const json = boost::json::parse(jsonStr).as_object();
    auto const parsingErrors = config_.parse(ConfigFileJson{json});
    ASSERT_FALSE(parsingErrors.has_value());

    auto const result = LogService::init(config_);
    EXPECT_FALSE(result);
    EXPECT_EQ(
        result.error(), "Can't override settings for log channel SomeChannel: invalid channel"
    );
}

TEST_F(LogServiceInitTests, LogSizeAndHourRotationCannotBeZero)
{
    std::vector<std::string_view> const keys{"log.directory_max_files", "log.rotation_size"};

    auto const jsonStr = fmt::format(
        R"JSON(
            {{
                "{}": 0,
                "{}": 0
            }}
        )JSON",
        keys[0],
        keys[1]
    );

    auto const parsingErrors =
        config_.parse(ConfigFileJson{boost::json::parse(jsonStr).as_object()});
    ASSERT_EQ(parsingErrors->size(), 2);
    for (std::size_t i = 0; i < parsingErrors->size(); ++i) {
        EXPECT_EQ(
            (*parsingErrors)[i].error,
            fmt::format(
                "{} Number must be between 1 and {}", keys[i], std::numeric_limits<uint32_t>::max()
            )
        );
    }
}
