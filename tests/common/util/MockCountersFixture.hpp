#pragma once

#include "util/LoggerFixtures.hpp"
#include "util/MockCounters.hpp"

/**
 * @brief Fixture with mock counters
 */
struct MockCountersTest : virtual public ::testing::Test {
protected:
    std::shared_ptr<MockCounters> mockCountersPtr_ = std::make_shared<MockCounters>();
};
