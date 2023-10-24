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

#include <util/Fixtures.h>
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
