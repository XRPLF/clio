#include "util/TimeUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/chrono.h>

#include <chrono>
#include <ctime>
#include <string>

TEST(TimeUtilTests, SystemTpFromUTCStrSuccess)
{
    auto const tp = util::systemTpFromUtcStr("2024-01-01T10:50:40Z", "%Y-%m-%dT%H:%M:%SZ");
    ASSERT_TRUE(tp.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto const time = std::chrono::system_clock::to_time_t(tp.value());
    std::tm timeStruct{};
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
    auto const tp = util::systemTpFromUtcStr("2024-01-01T", "%Y-%m-%dT%H:%M:%SZ");
    ASSERT_FALSE(tp.has_value());
}

TEST(TimeUtilTests, SystemTpToUtcStr)
{
    std::tm timeStruct{};
    timeStruct.tm_year = 123;  // 2023 (years since 1900)
    timeStruct.tm_mon = 9;     // October (0-based)
    timeStruct.tm_mday = 15;
    timeStruct.tm_hour = 14;
    timeStruct.tm_min = 30;
    timeStruct.tm_sec = 45;
    auto timePoint = std::chrono::system_clock::from_time_t(timegm(&timeStruct));

    std::string const isoFormat = "%Y-%m-%dT%H:%M:%SZ";
    std::string const isoStr = util::systemTpToUtcStr(timePoint, isoFormat);
    EXPECT_EQ(isoStr, "2023-10-15T14:30:45Z");

    std::string const customFormat = "%d/%m/%Y %H:%M:%S";
    std::string const customStr = util::systemTpToUtcStr(timePoint, customFormat);
    EXPECT_EQ(customStr, "15/10/2023 14:30:45");
}

TEST(TimeUtilTests, StringToTimePointToString)
{
    std::string const isoFormat = "%Y-%m-%dT%H:%M:%SZ";
    std::string const originalStr = "2023-10-15T14:30:45Z";
    auto timePoint = util::systemTpFromUtcStr(originalStr, isoFormat);
    ASSERT_TRUE(timePoint.has_value());

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    std::string const convertedStr = util::systemTpToUtcStr(*timePoint, isoFormat);
    EXPECT_EQ(originalStr, convertedStr);

    std::string const customFormat = "%d/%m/%Y %H:%M:%S";
    std::string const originalCustomStr = "15/10/2023 14:30:45";
    auto timePoint2 = util::systemTpFromUtcStr(originalCustomStr, customFormat);
    ASSERT_TRUE(timePoint2.has_value());

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    std::string const convertedCustomStr = util::systemTpToUtcStr(*timePoint2, customFormat);
    EXPECT_EQ(originalCustomStr, convertedCustomStr);

    EXPECT_EQ(*timePoint, *timePoint2);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TimeUtilTests, SystemTpFromLedgerCloseTime)
{
    using namespace std::chrono;

    auto const tp = util::systemTpFromLedgerCloseTime(ripple::NetClock::time_point{seconds{0}});
    EXPECT_EQ(tp.time_since_epoch(), ripple::epoch_offset);
}
