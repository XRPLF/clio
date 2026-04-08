#pragma once

#include "util/Assert.hpp"

#include <cstddef>
#include <functional>
#include <iterator>

namespace util {

/**
 * @brief Iterate over a container in batches
 *
 * @param container The container to iterate over
 * @param batchSize The size of each batch
 * @param fn The function to call for each batch
 */
void
forEachBatch(std::ranges::forward_range auto&& container, std::size_t batchSize, auto&& fn)
{
    ASSERT(batchSize > 0, "Batch size must be greater than 0");

    auto to = std::begin(container);
    auto end = std::end(container);

    while (to != end) {
        auto from = to;

        auto cnt = batchSize;
        while (to != end and cnt > 0) {
            ++to;
            --cnt;
        }

        std::invoke(fn, from, to);
    }
}

}  // namespace util
