//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/async/AnyStrand.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

/**
 * @brief A wrapper for std::priority_queue that serialises operations using a strand
 * @note This may be a candidate for future improvements if performance proves to be poor (e.g. use a lock free queue)
 */
template <typename T, typename Compare = std::less<T>>
class StrandedPriorityQueue {
    util::async::AnyStrand strand_;
    std::size_t limit_;
    std::priority_queue<T, std::vector<T>, Compare> queue_;

public:
    /**
     * @brief Construct a new priority queue on a strand
     * @param strand The strand to use
     * @param limit The limit of items allowed simultaniously in the queue
     */
    StrandedPriorityQueue(util::async::AnyStrand&& strand, std::optional<std::size_t> limit = std::nullopt)
        : strand_(std::move(strand)), limit_(limit.value_or(0uz))
    {
    }

    /**
     * @brief Enqueue a new item onto the queue if space is available
     * @note This function blocks until the item is attempted to be added to the queue
     *
     * @tparam I Type of the item to add
     * @param item The item to add
     * @return true if item added to the queue; false otherwise
     */
    template <typename I>
    [[nodiscard]] bool
    enqueue(I&& item)
        requires std::is_same_v<std::decay_t<I>, T>
    {
        return strand_
            .execute([&item, this] {
                if (limit_ == 0uz or queue_.size() < limit_) {
                    queue_.push(std::forward<I>(item));
                    return true;
                }
                return false;
            })
            .get()
            .value_or(false);  // if some exception happens - failed to add
    }

    /**
     * @brief Dequeue the next available item out of the queue
     * @note This function blocks until the item is taken off the queue
     * @return An item if available; nullopt otherwise
     */
    [[nodiscard]] std::optional<T>
    dequeue()
    {
        return strand_
            .execute([this] -> std::optional<T> {
                std::optional<T> out;

                if (not queue_.empty()) {
                    out.emplace(queue_.top());
                    queue_.pop();
                }

                return out;
            })
            .get()
            .value_or(std::nullopt);
    }

    /**
     * @brief Check if the queue is empty
     * @note This function blocks until the queue is checked
     *
     * @return true if the queue is empty; false otherwise
     */
    [[nodiscard]] bool
    empty()
    {
        return strand_.execute([this] { return queue_.empty(); }).get().value();
    }
};

}  // namespace util
