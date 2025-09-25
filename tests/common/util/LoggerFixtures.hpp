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
     * @brief Sets up spdlog loggers for each channel. Should be called once before using any loggers.
     * Simulates the `util::LogService::init(config)` call
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
