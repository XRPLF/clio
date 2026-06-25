#include "etl/CorruptionDetector.hpp"
#include "etl/SystemState.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace data;
using namespace util::prometheus;
using namespace testing;

struct CorruptionDetectorTest : WithPrometheus {};

TEST_F(CorruptionDetectorTest, DisableCacheOnCorruption)
{
    using namespace xrpl;
    auto state = etl::SystemState{};
    auto cache = MockLedgerCache{};
    auto detector = etl::CorruptionDetector{state, cache};

    EXPECT_CALL(cache, setDisabled()).Times(1);

    detector.onCorruptionDetected();

    EXPECT_TRUE(state.isCorruptionDetected);
}
