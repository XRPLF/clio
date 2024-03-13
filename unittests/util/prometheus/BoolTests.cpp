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
    Bool realBool{gauge};
    EXPECT_FALSE(realBool);
}
