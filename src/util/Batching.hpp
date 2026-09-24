#pragma once

#include "util/Assert.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

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

/**
 * @brief Accumulates a stream of records and flushes them to a sink in batches.
 *
 * Where @ref forEachBatch splits an already-complete container, this buffers records arriving over
 * many @ref add calls and invokes the sink once a full batch has accumulated, coalescing many small
 * inputs into fewer, larger writes. Any partial final batch is flushed by @ref flush. It is
 * thread-safe: @ref add may be called concurrently, and the sink is invoked outside the internal
 * lock so batch writes can proceed in parallel with further accumulation. A single @ref add may
 * push the buffer past the batch size, in which case the whole buffer is flushed as one batch.
 *
 * @tparam T The record type being batched.
 */
template <typename T>
class BatchBuffer {
    std::mutex mutex_;
    std::vector<T> buffer_;
    std::size_t batchSize_;
    std::function<void(std::vector<T> const&)> sink_;

public:
    /**
     * @brief Construct a batch buffer.
     *
     * @param batchSize The buffered-record count that triggers a flush; must be greater than zero.
     * @param sink The callback invoked with each full batch, and with the final partial batch on
     * @ref flush.
     */
    BatchBuffer(std::size_t batchSize, std::function<void(std::vector<T> const&)> sink)
        : batchSize_{batchSize}, sink_{std::move(sink)}
    {
        ASSERT(batchSize_ > 0, "Batch size must be greater than 0");
    }

    /**
     * @brief Append records, flushing a full batch to the sink once the threshold is reached.
     *
     * @param records The records to append; moved into the buffer.
     */
    void
    add(std::vector<T> records)
    {
        std::vector<T> batch;
        {
            std::scoped_lock const lock{mutex_};
            buffer_.insert(
                buffer_.end(),
                std::make_move_iterator(records.begin()),
                std::make_move_iterator(records.end())
            );
            if (buffer_.size() < batchSize_)
                return;
            batch = std::exchange(buffer_, {});
        }
        sink_(batch);
    }

    /**
     * @brief Flush any remaining buffered records to the sink.
     *
     * Does not invoke the sink when the buffer is empty.
     */
    void
    flush()
    {
        std::vector<T> batch;
        {
            std::scoped_lock const lock{mutex_};
            batch = std::exchange(buffer_, {});
        }
        if (not batch.empty())
            sink_(batch);
    }
};

}  // namespace util
