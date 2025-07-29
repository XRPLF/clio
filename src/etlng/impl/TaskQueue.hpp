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

#include "etlng/Models.hpp"
#include "util/Mutex.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace etlng::impl {

struct ReverseOrderComparator {
    [[nodiscard]] bool
    operator()(model::LedgerData const& lhs, model::LedgerData const& rhs) const noexcept
    {
        return lhs.seq > rhs.seq;
    }
};

/**
 * @brief A wrapper for std::priority_queue that serialises operations using a mutex
 * @note This may be a candidate for future improvements if performance proves to be poor (e.g. use a lock free queue)
 */
class TaskQueue {
    std::size_t limit_;
    std::uint32_t increment_;

    struct Data {
        std::uint32_t expectedSequence;
        std::priority_queue<model::LedgerData, std::vector<model::LedgerData>, ReverseOrderComparator> forwardLoadQueue;

        Data(std::uint32_t seq) : expectedSequence(seq)
        {
        }
    };

    util::Mutex<Data> data_;

public:
    struct Settings {
        std::uint32_t startSeq = 0u;   // sequence to start from (for dequeue)
        std::uint32_t increment = 1u;  // increment sequence by this value once dequeue was successful
        std::optional<std::size_t> limit = std::nullopt;
    };

    /**
     * @brief Construct a new priority queue
     * @param settings Settings for the queue, including starting sequence, increment value, and optional limit
     * @note If limit is not set, the queue will have no limit
     */
    explicit TaskQueue(Settings settings)
        : limit_(settings.limit.value_or(0uz)), increment_(settings.increment), data_(settings.startSeq)
    {
    }

    /**
     * @brief Enqueue a new item onto the queue if space is available
     * @note This function blocks until the item is attempted to be added to the queue
     *
     * @param item The item to add
     * @return true if item added to the queue; false otherwise
     */
    [[nodiscard]] bool
    enqueue(model::LedgerData item)
    {
        auto lock = data_.lock();

        if (limit_ == 0uz or lock->forwardLoadQueue.size() < limit_) {
            lock->forwardLoadQueue.push(std::move(item));
            return true;
        }

        return false;
    }

    /**
     * @brief Dequeue the next available item out of the queue
     * @note This function blocks until the item is taken off the queue
     * @return An item if available; nullopt otherwise
     */
    [[nodiscard]] std::optional<model::LedgerData>
    dequeue()
    {
        auto lock = data_.lock();
        std::optional<model::LedgerData> out;

        if (not lock->forwardLoadQueue.empty() && lock->forwardLoadQueue.top().seq == lock->expectedSequence) {
            out.emplace(lock->forwardLoadQueue.top());
            lock->forwardLoadQueue.pop();
            lock->expectedSequence += increment_;
        }

        return out;
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
        return data_.lock()->forwardLoadQueue.empty();
    }
};

}  // namespace etlng::impl
