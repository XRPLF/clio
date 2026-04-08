#pragma once

#include "util/Assert.hpp"

#include <cstddef>
#include <random>

namespace util {

/**
 * @brief Random number generator interface
 */
class RandomGeneratorInterface {
public:
    virtual ~RandomGeneratorInterface() = default;

    using SeedType = typename std::mt19937_64::result_type;

    /**
     * @brief Generate a random number between min and max
     *
     * @param min Minimum value
     * @param max Maximum value
     * @return Random number between min and max
     */
    [[nodiscard]]
    virtual size_t
    uniform(size_t min, size_t max) = 0;

    /**
     * @brief Set the seed for the random number generator
     *
     * @param seed Seed to set
     */
    virtual void
    setSeed(SeedType seed) = 0;
};

/**
 * @brief Mersenne Twister random number generator
 */
class MTRandomGenerator : public RandomGeneratorInterface {
public:
    MTRandomGenerator();

    /**
     * @brief Generate a random number between min and max
     *
     * @param min Minimum value
     * @param max Maximum value
     * @return Random number between min and max
     */
    [[nodiscard]]
    size_t
    uniform(size_t min, size_t max) override;

    /**
     * @brief Generate a random number between min and max
     *
     * @tparam T Type of the number to generate
     * @param min Minimum value
     * @param max Maximum value
     * @return Random number between min and max
     */
    template <typename T>
    [[nodiscard]]
    T
    uniformImpl(T min, T max)
    {
        ASSERT(min <= max, "Min cannot be greater than max. min: {}, max: {}", min, max);
        if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> distribution(min, max);
            return distribution(generator_);
        }
        std::uniform_int_distribution<T> distribution(min, max);
        return distribution(generator_);
    }

    /**
     * @brief Set the seed for the random number generator
     *
     * @param seed Seed to set
     */
    void
    setSeed(SeedType seed) override;

private:
    std::mt19937_64 generator_;
};

}  // namespace util
