#pragma once

#include "util/Random.hpp"

#include <gtest/gtest.h>

struct MockRandomGeneratorImpl : public util::RandomGeneratorInterface {
    MOCK_METHOD(size_t, uniform, (size_t min, size_t max), (override));
    MOCK_METHOD(void, setSeed, (SeedType seed), (override));
};

using MockRandomGenerator = testing::NiceMock<MockRandomGeneratorImpl>;
using StrictMockRandomGenerator = testing::StrictMock<MockRandomGeneratorImpl>;
