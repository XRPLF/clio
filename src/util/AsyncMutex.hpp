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

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

namespace util {

class AsyncMutex;

/**
 * @class AsyncMutexLock
 * @brief RAII wrapper for AsyncMutex that automatically unlocks the mutex on destruction
 *
 * This class is not copyable or movable to ensure proper locking semantics.
 * It is intended to be used with AsyncMutex::lock() which returns an instance of this class.
 */
class AsyncMutexLock {
    AsyncMutex& mutex_;

public:
    ~AsyncMutexLock();
    AsyncMutexLock(AsyncMutexLock&&) = delete;
    AsyncMutexLock(AsyncMutexLock const&) = delete;

private:
    friend AsyncMutex;
    AsyncMutexLock(AsyncMutex& mutex);
};

/**
 * @class AsyncMutex
 * @brief A mutex implementation that works with boost::asio coroutines
 *
 * AsyncMutex provides mutual exclusion for asynchronous code using boost::asio coroutines.
 * Instead of blocking threads, it suspends coroutines when the mutex is already locked.
 *
 * Usage example:
 * @code
 * AsyncMutex mutex(executor);
 *
 * // Inside a coroutine:
 * auto lock = mutex.lock(yield); // Suspends coroutine if mutex is already locked
 * // Critical section - only one coroutine can execute this at a time
 * // Lock is automatically released when it goes out of scope
 * @endcode
 *
 * @note This class should be used only with the same coroutine or its child coroutines.
 * Using this mutex from different coroutines may lead to a race condition.
 */
class AsyncMutex {
    friend AsyncMutexLock;
    bool locked_ = false;
    boost::asio::steady_timer timer_;

public:
    /**
     * @brief Constructs an AsyncMutex
     * @param executor The boost::asio executor to use for async operations
     */
    AsyncMutex(boost::asio::any_io_executor executor);

    ~AsyncMutex() = default;

    /** @brief AsyncMutex is safe to move when it is not locked (there is an assert inside) */
    AsyncMutex(AsyncMutex&&);

    AsyncMutex(AsyncMutex const&) = delete;

    /**
     * @brief Acquires the mutex, suspending the coroutine if the mutex is already locked
     * @param yield The boost::asio yield context from the calling coroutine
     * @return An AsyncMutexLock that will automatically release the mutex when destroyed
     *
     * If the mutex is already locked, this function will suspend the current coroutine
     * until the mutex becomes available. Once acquired, the mutex remains locked until
     * the returned AsyncMutexLock is destroyed.
     */
    [[nodiscard]] AsyncMutexLock
    lock(boost::asio::yield_context yield);

private:
    /**
     * @brief Releases the mutex
     *
     * This is called automatically by AsyncMutexLock's destructor and should not be called directly.
     */
    void
    unlock();
};

}  // namespace util
