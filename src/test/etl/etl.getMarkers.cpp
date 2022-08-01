#include <gtest/gtest.h>

#include <clio/etl/ETLHelpers.h>
#include <thread>

TEST(networkValidatedLedgers, get16Markers)
{
    auto markers = getMarkers(16);

    ASSERT_EQ(markers.size(), 16);

    auto last = markers[0];
    for (auto i = 1; i < 15; ++i)
    {
        ASSERT_TRUE(last < markers[i]);
        ASSERT_EQ(markers[i].data()[0] - last.data()[0], 16);
        last = markers[i];
    }
}

TEST(networkValidatedLedgers, get2Markers)
{
    auto markers = getMarkers(2);

    ASSERT_EQ(markers.size(), 2);

    auto last = markers[0];
    for (auto i = 1; i < 2; ++i)
    {
        ASSERT_TRUE(last < markers[i]);
        ASSERT_EQ(markers[i].data()[0] - last.data()[0], 128);
        last = markers[i];
    }
}

TEST(networkValidatedLedgers, get64Markers)
{
    auto markers = getMarkers(64);

    ASSERT_EQ(markers.size(), 64);

    auto last = markers[0];
    for (auto i = 1; i < 64; ++i)
    {
        ASSERT_TRUE(last < markers[i]);
        ASSERT_EQ(markers[i].data()[0] - last.data()[0], 4);
        last = markers[i];
    }
}
