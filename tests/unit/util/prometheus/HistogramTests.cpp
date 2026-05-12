#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/OStream.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
        MOCK_METHOD(void, serializeValue, (std::string const&, std::string, OStream&), (const));
    };

    ::testing::StrictMock<MockHistogramImpl> mockHistogramImpl;
    std::string const name = "test_histogram";
    std::string labelsString = R"JSON({label1="value1",label2="value2"})JSON";
    HistogramInt histogram{
        name,
        labelsString,
        {1, 2, 3},
        static_cast<MockHistogramImpl&>(mockHistogramImpl)
    };
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
    EXPECT_CALL(mockHistogramImpl, serializeValue(name, labelsString, ::testing::_));
    histogram.serializeValue(stream);
}

struct HistogramTests : ::testing::Test {
    std::string labelsString = R"JSON({label1="value1",label2="value2"})JSON";
    HistogramInt histogram{"t", labelsString, {1, 2, 3}};

    [[nodiscard]] std::string
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
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"1\"} 1\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"2\"} 1\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"3\"} 1\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"+Inf\"} 1\n"
        "t_sum{label1=\"value1\",label2=\"value2\"} 0\n"
        "t_count{label1=\"value1\",label2=\"value2\"} 1\n"
    ) << serialize();

    histogram.observe(2);
    EXPECT_EQ(
        serialize(),
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"1\"} 1\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"2\"} 2\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"3\"} 2\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"+Inf\"} 2\n"
        "t_sum{label1=\"value1\",label2=\"value2\"} 2\n"
        "t_count{label1=\"value1\",label2=\"value2\"} 2\n"
    );

    histogram.observe(123);
    EXPECT_EQ(
        serialize(),
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"1\"} 1\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"2\"} 2\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"3\"} 2\n"
        "t_bucket{label1=\"value1\",label2=\"value2\",le=\"+Inf\"} 3\n"
        "t_sum{label1=\"value1\",label2=\"value2\"} 125\n"
        "t_count{label1=\"value1\",label2=\"value2\"} 3\n"
    );
}
