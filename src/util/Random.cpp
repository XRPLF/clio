#include "util/Random.hpp"

#include <chrono>
#include <cstddef>
#include <random>

namespace util {

MTRandomGenerator::MTRandomGenerator()
    : generator_{std::chrono::system_clock::now().time_since_epoch().count()}
{
}

size_t
MTRandomGenerator::uniform(size_t min, size_t max)
{
    return uniformImpl(min, max);
}

void
MTRandomGenerator::setSeed(SeedType seed)
{
    generator_.seed(seed);
}

}  // namespace util
