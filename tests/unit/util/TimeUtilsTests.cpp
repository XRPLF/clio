//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/TimeUtils.hpp"

#include <gtest/gtest.h>

TEST(TimeUtilTests, SystemTpFromUTCStrSuccess)
{
    auto const tp = util::SystemTpFromUTCStr("2024-01-01T10:50:40Z", "%Y-%m-%dT%H:%M:%SZ");
    ASSERT_TRUE(tp.has_value());
    auto const time = std::chrono::system_clock::to_time_t(tp.value());
    std::tm timeStruct;
    gmtime_r(&time, &timeStruct);
    EXPECT_EQ(timeStruct.tm_year + 1900, 2024);
    EXPECT_EQ(timeStruct.tm_mon, 0);
    EXPECT_EQ(timeStruct.tm_mday, 1);
    EXPECT_EQ(timeStruct.tm_hour, 10);
    EXPECT_EQ(timeStruct.tm_min, 50);
    EXPECT_EQ(timeStruct.tm_sec, 40);
}

TEST(TimeUtilTests, SystemTpFromUTCStrFail)
{
    auto const tp = util::SystemTpFromUTCStr("2024-01-01T", "%Y-%m-%dT%H:%M:%SZ");
    ASSERT_FALSE(tp.has_value());
}

TEST(TimeUtilTests, SystemTpFromLedgerCloseTime)
{
    using namespace std::chrono;

    auto const tp = util::SystemTpFromLedgerCloseTime(ripple::NetClock::time_point{seconds{0}});
    EXPECT_EQ(tp.time_since_epoch(), ripple::epoch_offset);
}
