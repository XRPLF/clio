//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <ripple/basics/base_uint.h>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>

namespace etl {
/**
 * @brief This datastructure is used to keep track of the sequence of the most recent ledger validated by the network.
 *
 * There are two methods that will wait until certain conditions are met. This datastructure is able to be "stopped".
 * When the datastructure is stopped, any threads currently waiting are unblocked.
 * Any later calls to methods of this datastructure will not wait. Once the datastructure is stopped, the datastructure
 * remains stopped for the rest of its lifetime.
 */
class NetworkValidatedLedgers
{
    // max sequence validated by network
    std::optional<uint32_t> max_;

    mutable std::mutex m_;
    std::condition_variable cv_;

public:
    static std::shared_ptr<NetworkValidatedLedgers>
    make_ValidatedLedgers()
    {
        return std::make_shared<NetworkValidatedLedgers>();
    }

    /**
     * @brief Notify the datastructure that idx has been validated by the network
     *
     * @param idx sequence validated by network
     */
    void
    push(uint32_t idx)
    {
        std::lock_guard lck(m_);
        if (!max_ || idx > *max_)
            max_ = idx;
        cv_.notify_all();
    }

    /**
     * @brief Get most recently validated sequence.
     *
     * If no ledgers are known to have been validated, this function waits until the next ledger is validated
     *
     * @return sequence of most recently validated ledger. empty optional if the datastructure has been stopped
     */
    std::optional<uint32_t>
    getMostRecent()
    {
        std::unique_lock lck(m_);
        cv_.wait(lck, [this]() { return max_; });
        return max_;
    }

    /**
     * @brief Waits for the sequence to be validated by the network
     *
     * @param sequence to wait for
     * @return true if sequence was validated, false otherwise a return value of false means the datastructure has been
     * stopped
     */
    bool
    waitUntilValidatedByNetwork(uint32_t sequence, std::optional<uint32_t> maxWaitMs = {})
    {
        std::unique_lock lck(m_);
        auto pred = [sequence, this]() -> bool { return (max_ && sequence <= *max_); };
        if (maxWaitMs)
            cv_.wait_for(lck, std::chrono::milliseconds(*maxWaitMs));
        else
            cv_.wait(lck, pred);
        return pred();
    }
};

// TODO: does the note make sense? lockfree queues provide the same blocking behaviour just without mutex, don't they?
/**
 * @brief Generic thread-safe queue with a max capacity
 *
 * @note (original note) We can't use a lockfree queue here, since we need the ability to wait for an element to be
 * added or removed from the queue. These waits are blocking calls.
 */
template <class T>
class ThreadSafeQueue
{
    std::queue<T> queue_;

    mutable std::mutex m_;
    std::condition_variable cv_;
    uint32_t maxSize_;

public:
    /**
     * @brief Create an instance of the queue
     *
     * @param maxSize maximum size of the queue. Calls that would cause the queue to exceed this size will block until
     * free space is available
     */
    ThreadSafeQueue(uint32_t maxSize) : maxSize_(maxSize)
    {
    }

    /**
     * @brief Push element onto the queue
     *
     * Note: This method will block until free space is available
     *
     * @param elt element to push onto queue
     */
    void
    push(T const& elt)
    {
        std::unique_lock lck(m_);
        cv_.wait(lck, [this]() { return queue_.size() <= maxSize_; });
        queue_.push(elt);
        cv_.notify_all();
    }

    /**
     * @brief Push element onto the queue
     *
     * Note: This method will block until free space is available
     *
     * @param elt element to push onto queue. elt is moved from
     */
    void
    push(T&& elt)
    {
        std::unique_lock lck(m_);
        cv_.wait(lck, [this]() { return queue_.size() <= maxSize_; });
        queue_.push(std::move(elt));
        cv_.notify_all();
    }

    /**
     * @brief Pop element from the queue
     *
     * Note: Will block until queue is non-empty
     *
     * @return element popped from queue
     */
    T
    pop()
    {
        std::unique_lock lck(m_);
        cv_.wait(lck, [this]() { return !queue_.empty(); });

        T ret = std::move(queue_.front());
        queue_.pop();

        cv_.notify_all();
        return ret;
    }

    /**
     * @brief Attempt to pop an element
     *
     * @return element popped from queue or empty optional if queue was empty
     */
    std::optional<T>
    tryPop()
    {
        std::scoped_lock lck(m_);
        if (queue_.empty())
            return {};

        T ret = std::move(queue_.front());
        queue_.pop();

        cv_.notify_all();
        return ret;
    }
};

/**
 * @brief Parititions the uint256 keyspace into numMarkers partitions, each of equal size.
 *
 * @param numMarkers total markers to partition for
 */
inline std::vector<ripple::uint256>
getMarkers(size_t numMarkers)
{
    assert(numMarkers <= 256);

    unsigned char incr = 256 / numMarkers;

    std::vector<ripple::uint256> markers;
    markers.reserve(numMarkers);
    ripple::uint256 base{0};
    for (size_t i = 0; i < numMarkers; ++i)
    {
        markers.push_back(base);
        base.data()[0] += incr;
    }
    return markers;
}
}  // namespace etl