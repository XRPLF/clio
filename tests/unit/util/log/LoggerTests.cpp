#include "util/LoggerFixtures.hpp"
#include "util/log/Logger.hpp"

#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <memory>
#include <string>
using namespace util;

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
