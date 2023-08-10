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

#include <rpc/WorkQueue.h>

#include <boost/json.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace RPC {

/**
 * @brief Holds information about successful, failed, forwarded, etc. RPC handler calls.
 */
class Counters
{
    /**
     * @brief All counters the system keeps track of for each RPC method.
     */
    struct MethodInfo
    {
        std::uint64_t started = 0u;
        std::uint64_t finished = 0u;
        std::uint64_t failed = 0u;
        std::uint64_t errored = 0u;
        std::uint64_t forwarded = 0u;
        std::uint64_t failedForward = 0u;
        std::uint64_t duration = 0u;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MethodInfo> methodInfo_;

    // counters that don't carry RPC method information
    std::atomic_uint64_t tooBusyCounter_;
    std::atomic_uint64_t notReadyCounter_;
    std::atomic_uint64_t badSyntaxCounter_;
    std::atomic_uint64_t unknownCommandCounter_;
    std::atomic_uint64_t internalErrorCounter_;

    std::reference_wrapper<const WorkQueue> workQueue_;
    std::chrono::time_point<std::chrono::system_clock> startupTime_;

public:
    /**
     * @brief Creates a new counters instance that operates on the given WorkQueue.
     *
     * @param wq The work queue to operate on
     */
    Counters(WorkQueue const& wq) : workQueue_(std::cref(wq)), startupTime_{std::chrono::system_clock::now()} {};

    /**
     * @brief A factory function that creates a new counters instance.
     *
     * @param wq The work queue to operate on
     * @return The new instance
     */
    static Counters
    make_Counters(WorkQueue const& wq)
    {
        return Counters{wq};
    }

    /**
     * @brief Increments the failed count for a particular RPC method.
     *
     * @param method The method to increment the count for
     */
    void
    rpcFailed(std::string const& method);

    /**
     * @brief Increments the errored count for a particular RPC method.
     *
     * @param method The method to increment the count for
     */
    void
    rpcErrored(std::string const& method);

    /**
     * @brief Increments the completed count for a particular RPC method.
     *
     * @param method The method to increment the count for
     */
    void
    rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration);

    /**
     * @brief Increments the forwarded count for a particular RPC method.
     *
     * @param method The method to increment the count for
     */
    void
    rpcForwarded(std::string const& method);

    /**
     * @brief Increments the failed to forward count for a particular RPC method.
     *
     * @param method The method to increment the count for
     */
    void
    rpcFailedToForward(std::string const& method);

    /*! @brief Increments the global too busy counter. */
    void
    onTooBusy();

    /*! @brief Increments the global not ready counter. */
    void
    onNotReady();

    /*! @brief Increments the global bad syntax counter. */
    void
    onBadSyntax();

    /*! @brief Increments the global unknown command/method counter. */
    void
    onUnknownCommand();

    /*! @brief Increments the global internal error counter. */
    void
    onInternalError();

    /*! @return Uptime of this instance in seconds. */
    std::chrono::seconds
    uptime() const;

    /*! @return A JSON report with current state of all counters for every method. */
    boost::json::object
    report() const;
};

}  // namespace RPC
