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

#include "util/StringBuffer.hpp"

#include <gtest/gtest.h>

#include <ostream>
#include <string>

/**
 * @brief A fixture for testing LogService and Logger.
 */
class LoggerFixture : virtual public ::testing::Test {
    StringBuffer buffer_;
    std::ostream stream_ = std::ostream{&buffer_};

public:
    // Simulates the `util::LogService::init(config)` call
    LoggerFixture();

protected:
    std::string
    getLoggerString()
    {
        return buffer_.getStrAndReset();
    }
};

/**
 * @brief Fixture with util::Logger support but completely disabled logging.
 *
 * This is meant to be used as a base for other fixtures.
 */
struct NoLoggerFixture : virtual LoggerFixture {
    NoLoggerFixture();
};
