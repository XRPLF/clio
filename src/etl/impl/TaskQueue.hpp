#pragma once

#include "etl/Models.hpp"
#include "util/Assert.hpp"
#include "util/Mutex.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace etl::impl {

struct ReverseOrderComparator {
    [[nodiscard]] bool
    operator()(model::LedgerData const& lhs, model::LedgerData const& rhs) const noexcept
    {
        return lhs.seq > rhs.seq;
    }
};

/**
 * @brief A wrapper for std::priority_queue that serialises operations using a mutex
 * @note This may be a candidate for future improvements if performance proves to be poor (e.g. use
 * a lock free queue)
 */
class TaskQueue {
    struct Data {
        std::uint32_t expectedSequence;
        std::priority_queue<
            model::LedgerData,
            std::vector<model::LedgerData>,
            ReverseOrderComparator>
            forwardLoadQueue;

        Data(std::uint32_t seq) : expectedSequence(seq)
        {
        }
    };

    std::size_t limit_;
    std::uint32_t increment_;
    util::Mutex<Data> data_;

    std::condition_variable cv_;
    std::atomic_bool stopping_ = false;

public:
    struct Settings {
        std::uint32_t startSeq = 0u;  // sequence to start from (for dequeue)
        std::uint32_t increment =
            1u;  // increment sequence by this value once dequeue was successful
        std::optional<std::size_t> limit = std::nullopt;
    };

    /**
     * @brief Construct a new priority queue
     * @param settings Settings for the queue, including starting sequence, increment value, and
     * optional limit
     * @note If limit is not set, the queue will have no limit
     */
    explicit TaskQueue(Settings settings)
        : limit_(settings.limit.value_or(0uz))
        , increment_(settings.increment)
        , data_(settings.startSeq)
    {
    }

    ~TaskQueue()
    {
        ASSERT(stopping_, "stop() must be called before destroying the TaskQueue");
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
            cv_.notify_all();

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

        if (not lock->forwardLoadQueue.empty() &&
            lock->forwardLoadQueue.top().seq == lock->expectedSequence) {
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

    /**
     * @brief Awaits for the queue to become non-empty
     * @note This function blocks until there is a task or the queue is being destroyed
     */
    void
    awaitTask()
    {
        if (stopping_)
            return;

        auto lock = data_.lock<std::unique_lock>();
        cv_.wait(lock, [&] { return stopping_ or not lock->forwardLoadQueue.empty(); });
    }

    /**
     * @brief Notify the queue that it's no longer needed
     * @note This must be called before the queue is destroyed
     */
    void
    stop()
    {
        // unblock all waiters
        stopping_ = true;
        cv_.notify_all();
    }
};

}  // namespace etl::impl
