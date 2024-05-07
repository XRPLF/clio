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

#include "etl/CorruptionDetector.hpp"
#include "etl/SystemState.hpp"
#include "util/Fixtures.hpp"
#include "util/MockCache.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace data;
using namespace util::prometheus;
using namespace testing;

struct CorruptionDetectorTest : NoLoggerFixture, WithPrometheus {};

TEST_F(CorruptionDetectorTest, DisableCacheOnCorruption)
{
    using namespace ripple;
    auto state = etl::SystemState{};
    auto cache = MockCache{};
    auto detector = etl::CorruptionDetector{state, cache};

    EXPECT_CALL(cache, setDisabled()).Times(1);

    detector.onCorruptionDetected();

    EXPECT_TRUE(state.isCorruptionDetected);
}
