//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <util/prometheus/Histogram.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace util::prometheus;

struct AnyHistogramTests : ::testing::Test {
    struct MockHistogramImpl {
        MockHistogramImpl()
        {
            EXPECT_CALL(*this, setBuckets);
        }
        using ValueType = std::int64_t;
        MOCK_METHOD(void, observe, (ValueType));
        MOCK_METHOD(void, setBuckets, (std::vector<ValueType> const&));
        MOCK_METHOD(void, serializeValue, (std::string const&, OStream&), (const));
    };

    ::testing::StrictMock<MockHistogramImpl> mockHistogramImpl;
    std::string const name = "test_histogram";
    std::string labelsString = R"({label1="value1",label2="value2"})";
    HistogramInt histogram{name, labelsString, {1, 2, 3}, static_cast<MockHistogramImpl&>(mockHistogramImpl)};
};

TEST_F(AnyHistogramTests, name)
{
    EXPECT_EQ(histogram.name(), name);
}

TEST_F(AnyHistogramTests, labelsString)
{
    EXPECT_EQ(histogram.labelsString(), labelsString);
}

TEST_F(AnyHistogramTests, observe)
{
    EXPECT_CALL(mockHistogramImpl, observe(42));
    histogram.observe(42);
}

TEST_F(AnyHistogramTests, serializeValue)
{
    OStream stream{false};
    EXPECT_CALL(mockHistogramImpl, serializeValue(name, ::testing::_));
    histogram.serializeValue(stream);
}

struct HistogramTests : ::testing::Test {
    std::vector<std::int64_t> const buckets{1, 2, 3};
    HistogramInt histogram{"t", "", {1, 2, 3}};

    std::string
    serialize() const
    {
        OStream stream{false};
        histogram.serializeValue(stream);
        return std::move(stream).data();
    }
};

TEST_F(HistogramTests, observe)
{
    histogram.observe(0);
    EXPECT_EQ(
        serialize(),
        "t_bucket{le=\"1\"} 1\n"
        "t_bucket{le=\"2\"} 1\n"
        "t_bucket{le=\"3\"} 1\n"
        "t_bucket{le=\"+Inf\"} 1\n"
        "t_sum 0\n"
        "t_count 1\n"
    );

    histogram.observe(2);
    EXPECT_EQ(
        serialize(),
        "t_bucket{le=\"1\"} 1\n"
        "t_bucket{le=\"2\"} 2\n"
        "t_bucket{le=\"3\"} 2\n"
        "t_bucket{le=\"+Inf\"} 2\n"
        "t_sum 2\n"
        "t_count 2\n"
    );

    histogram.observe(123);
    EXPECT_EQ(
        serialize(),
        "t_bucket{le=\"1\"} 1\n"
        "t_bucket{le=\"2\"} 2\n"
        "t_bucket{le=\"3\"} 2\n"
        "t_bucket{le=\"+Inf\"} 3\n"
        "t_sum 125\n"
        "t_count 3\n"
    );
}
