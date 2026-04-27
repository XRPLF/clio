#include "util/LoggerFixtures.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
using namespace util;
using util::config::Array;
using util::config::ConfigFileJson;
using util::config::ConfigType;
using util::config::ConfigValue;

namespace {
size_t
loggersNum()
{
    size_t counter = 0;
    spdlog::apply_all([&counter](std::shared_ptr<spdlog::logger>) { ++counter; });
    return counter;
}
}  // namespace

// Used as a fixture for tests with enabled logging
class LoggerTest : public LoggerFixture {};

TEST_F(LoggerTest, Basic)
{
    Logger const log{"General"};

    log.info() << "Info line logged";
    ASSERT_EQ(getLoggerString(), "inf:General - Info line logged\n");

    LogService::debug() << "Debug line with numbers " << 12345;
    ASSERT_EQ(getLoggerString(), "deb:General - Debug line with numbers 12345\n");

    LogService::warn() << "Warning is logged";
    ASSERT_EQ(getLoggerString(), "war:General - Warning is logged\n");
}

TEST_F(LoggerTest, Filtering)
{
    Logger const log{"General"};
    log.trace() << "Should not be logged";
    ASSERT_TRUE(getLoggerString().empty());

    log.warn() << "Warning is logged";
    ASSERT_EQ(getLoggerString(), "war:General - Warning is logged\n");
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

TEST_F(LoggerTest, ManyDynamicLoggers)
{
    static constexpr size_t kNUM_LOGGERS = 10'000;

    auto initialLoggers = loggersNum();

    for (size_t i = 0; i < kNUM_LOGGERS; ++i) {
        std::string const loggerName = "DynamicLogger" + std::to_string(i);

        Logger const log{loggerName};
        log.info() << "Logger number " << i;
        ASSERT_EQ(
            getLoggerString(), "inf:" + loggerName + " - Logger number " + std::to_string(i) + "\n"
        );

        Logger const copy = log;
        copy.info() << "Copy of logger number " << i;
        ASSERT_EQ(
            getLoggerString(),
            "inf:" + loggerName + " - Copy of logger number " + std::to_string(i) + "\n"
        );
    }

    ASSERT_EQ(loggersNum(), initialLoggers);
}

/**
 * @brief Fixture for testing real log-file rotation behaviour.
 *
 * Unlike LoggerTest (which uses LoggerFixture's in-memory buffer), this fixture
 * initialises the LogService with a real file sink and redirects all spdlog
 * loggers to that sink so that written messages actually land on disk.
 */
struct LogFileRotationTests : ::testing::Test {
    std::filesystem::path const tmpDir = std::filesystem::temp_directory_path() /
        fmt::format("clio_log_rotation_tests_{}",
                    boost::uuids::to_string(boost::uuids::random_generator{}()));

    util::config::ClioConfigDefinition config{
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
             util::config::gValidateUint32
         )},

        {"log.directory_max_files",
         ConfigValue{ConfigType::Integer}.defaultValue(25).withConstraint(
             util::config::gValidateUint32
         )},

        {"log.rotate", ConfigValue{ConfigType::Boolean}.defaultValue(true)},

        {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
    };

    LogFileRotationTests()
    {
        std::filesystem::remove_all(tmpDir);
        if (LogServiceState::initialized())
            LogServiceState::reset();
    }

    ~LogFileRotationTests() override
    {
        if (LogService::initialized())
            LogService::reset();
        // Leave state initialised so that subsequent tests can call reset().
        LogServiceState::init(false, Severity::FTL, {});
        std::filesystem::remove_all(tmpDir);
    }

    /**
     * @brief Initialises LogService with the current config and redirects all
     * existing spdlog loggers to the newly created file sink.
     *
     * LogService::init() skips updating sinks on loggers that already exist in
     * the spdlog registry. Calling replaceSinks() here ensures every logger
     * writes to the file sink regardless of prior test state.
     */
    void
    initFileLogging() const
    {
        ASSERT_TRUE(LogService::init(config));
        LogServiceState::replaceSinks(LogServiceState::sinks_);
    }

    /** @brief Returns the number of regular files in tmpDir_. */
    [[nodiscard]] std::size_t
    countLogFiles() const
    {
        std::size_t count = 0;
        for (auto const& entry : std::filesystem::directory_iterator(tmpDir)) {
            if (entry.is_regular_file())
                ++count;
        }
        return count;
    }
};

TEST_F(LogFileRotationTests, RotationDisabledProducesSingleLogFile)
{
    auto const parsingErrors = config.parse(
        ConfigFileJson{boost::json::object{
            {"log",
             boost::json::object{
                 {"directory", tmpDir.string()},
                 {"rotate", false},
             }}
        }}
    );
    ASSERT_FALSE(parsingErrors.has_value());

    initFileLogging();

    // Write enough data to trigger rotation if it were enabled (> 1 MB).
    // Writing at error level flushes immediately because flush_on(err) is set.
    Logger const log{"General"};
    std::string const bigMessage(1000, 'x');
    for (int i = 0; i < 1100; ++i)
        log.error() << bigMessage;

    EXPECT_EQ(countLogFiles(), 1u);
}

TEST_F(LogFileRotationTests, RotationEnabledProducesMultipleLogFiles)
{
    auto const parsingErrors = config.parse(
        ConfigFileJson{boost::json::object{
            {"log",
             boost::json::object{
                 {"directory", tmpDir.string()},
                 {"rotate", true},
                 {"rotation_size", 1},
                 {"directory_max_files", 2},
             }}
        }}
    );
    ASSERT_FALSE(parsingErrors.has_value());

    initFileLogging();

    Logger const log{"General"};
    std::string const bigMessage(1000, 'x');
    for (int i = 0; i < 1100; ++i)
        log.error() << bigMessage;

    EXPECT_GT(countLogFiles(), 1u);
}
