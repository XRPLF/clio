#pragma once

#include "util/LoggerBuffer.hpp"

#include <gtest/gtest.h>

#include <string>

/**
 * @brief A fixture for testing LogService and Logger.
 */
class LoggerFixture : virtual public ::testing::Test {
protected:
    LoggerBuffer buffer_;

public:
    LoggerFixture();
    ~LoggerFixture() override;

    /**
     * @brief Sets up spdlog loggers for each channel. Should be called once before using any
     * loggers. Simulates the `util::LogService::init(config)` call
     */
    static void
    init();

protected:
    [[nodiscard]]
    std::string
    getLoggerString()
    {
        return buffer_.getStrAndReset();
    }

private:
    void
    resetTestingLoggers();
};
