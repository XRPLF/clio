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

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
using namespace util;

// Used as a fixture for tests with enabled logging
class LoggerTest : public LoggerFixture {};

// Used as a fixture for tests with disabled logging
class NoLoggerTest : public NoLoggerFixture {};

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

    Logger const tlog{"Trace"};
    tlog.trace() << "Trace line logged for 'Trace' component";
    ASSERT_EQ(getLoggerString(), "tra:Trace - Trace line logged for 'Trace' component\n");
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
    ASSERT_TRUE(getLoggerString().empty());

    LogService::fatal() << "Still nothing";
    ASSERT_TRUE(getLoggerString().empty());
}
