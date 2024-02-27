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
#pragma once

#include "util/Assert.hpp"

#include <random>

namespace util {

/**
 * @brief Random number generator
 */
class Random {
public:
    /**
     * @brief Generate a random number between min and max
     *
     * @tparam T Type of the number to generate
     * @param min Minimum value
     * @param max Maximum value
     * @return Random number between min and max
     */
    template <typename T>
    static T
    uniform(T min, T max)
    {
        ASSERT(min <= max, "Min cannot be greater than max. min: {}, max: {}", min, max);
        if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> distribution(min, max);
            return distribution(generator_);
        }
        std::uniform_int_distribution<T> distribution(min, max);
        return distribution(generator_);
    }

private:
    static std::mt19937_64 generator_;
};

}  // namespace util
