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

#include "util/Mutex.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <memory>

namespace cluster::impl {

/**
 * @brief One-shot timer for fallback recovery, with internal thread-safety.
 *
 * The timer state — including the @c boost::asio::steady_timer, the running flag,
 * and the recovery duration — is held inside a mutex-protected @c Impl struct
 * referenced via @c shared_ptr.  This makes @c FallbackRecoveryTimer cheap to
 * copy: all copies share the same underlying state.  All public methods are
 * thread-safe; callers do not need an external @c util::Mutex.
 */
class FallbackRecoveryTimer {
    struct Impl {
        boost::asio::steady_timer timer;
        bool isRunning = false;
        std::chrono::steady_clock::duration recoveryTime;

        Impl(
            boost::asio::any_io_executor const& executor,
            std::chrono::steady_clock::duration recoveryTime
        )
            : timer(executor), recoveryTime(recoveryTime)
        {
        }
    };

    std::shared_ptr<util::Mutex<Impl>> impl_;

public:
    /**
     * @brief Construct a timer bound to the given thread pool.
     *
     * @param ctx          Thread pool whose executor the timer is posted to.
     * @param recoveryTime Duration to wait before the timer fires.
     */
    FallbackRecoveryTimer(
        boost::asio::thread_pool& ctx,
        std::chrono::steady_clock::duration recoveryTime
    );

    /**
     * @brief Returns @c true if an @c async_wait is currently pending.
     */
    [[nodiscard]] bool
    isRunning() const;

    /**
     * @brief Arm the timer and schedule @p callback for when it fires.
     *
     * Sets the running flag, calls @c expires_after, and posts @c async_wait.  The
     * internal async_wait handler clears the running flag before forwarding the
     * @c error_code to @p callback.  If the timer is cancelled, @p callback receives
     * @c boost::asio::error::operation_aborted — callers must handle this.
     *
     * @param callback Invoked with the error code when the timer fires or is cancelled.
     */
    template <typename Callback>
    void
    start(Callback&& callback)
    {
        auto locked = impl_->lock();
        locked->isRunning = true;
        locked->timer.expires_after(locked->recoveryTime);
        locked->timer.async_wait([impl = impl_, cb = std::forward<Callback>(callback)](
                                     boost::system::error_code ec
                                 ) mutable {
            impl->lock()->isRunning = false;
            cb(ec);
        });
    }

    /**
     * @brief Cancel any pending @c async_wait and clear the running flag.
     */
    void
    cancel();
};

}  // namespace cluster::impl
