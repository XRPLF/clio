#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Gauge.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

using namespace util::prometheus;
using testing::StrictMock;

struct BoolTests : public testing::Test {
    struct MockImpl {
        MOCK_METHOD(void, set, (int64_t), ());
        MOCK_METHOD(int64_t, value, (), ());
    };

protected:
    StrictMock<MockImpl> impl_;
    AnyBool<StrictMock<MockImpl>> bool_{impl_};
};

TEST_F(BoolTests, Set)
{
    EXPECT_CALL(impl_, set(1));
    bool_ = true;

    EXPECT_CALL(impl_, set(0));
    bool_ = false;
}

TEST_F(BoolTests, Get)
{
    EXPECT_CALL(impl_, value()).WillOnce(testing::Return(1));
    EXPECT_TRUE(bool_);

    EXPECT_CALL(impl_, value()).WillOnce(testing::Return(0));
    EXPECT_FALSE(bool_);
}

TEST_F(BoolTests, DefaultValues)
{
    GaugeInt gauge{"test", ""};
    Bool const realBool{gauge};
    EXPECT_FALSE(realBool);
}
