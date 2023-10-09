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

#include <boost/json/object.hpp>

#include <atomic>
#include <memory>
#include <utility>

namespace data {

/**
 * @brief A concept for a class that can be used to count backend operations.
 */
// clang-format off
template <typename T>
concept SomeBackendCounters = requires(T a) {
    typename T::Ptr;
    { a.registerTooBusy() } -> std::same_as<void>;
    { a.registerWriteSync() } -> std::same_as<void>;
    { a.registerWriteSyncRetry() } -> std::same_as<void>;
    { a.registerWriteStarted() } -> std::same_as<void>;
    { a.registerWriteFinished() } -> std::same_as<void>;
    { a.registerWriteRetry() } -> std::same_as<void>;
    { a.registerReadStarted(std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadFinished(std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadRetry(std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadError(std::uint64_t{}) } -> std::same_as<void>;
    { a.report() } -> std::same_as<boost::json::object>;
};
// clang-format on

/**
 * @brief Holds statistics about the backend.
 * @note This class is thread-safe.
 */
class BackendCounters
{
public:
    using Ptr = std::shared_ptr<BackendCounters>;

    static Ptr
    make();

    void
    registerTooBusy();

    void
    registerWriteSync();

    void
    registerWriteSyncRetry();

    void
    registerWriteStarted();

    void
    registerWriteFinished();

    void
    registerWriteRetry();

    void
    registerReadStarted(std::uint64_t count = 1u);

    void
    registerReadFinished(std::uint64_t count = 1u);

    void
    registerReadRetry(std::uint64_t count = 1u);

    void
    registerReadError(std::uint64_t count = 1u);

    boost::json::object
    report() const;

private:
    BackendCounters() = default;

    class AsyncOperationCounters
    {
    public:
        AsyncOperationCounters(std::string name);

        void
        registerStarted(std::uint64_t count);

        void
        registerFinished(std::uint64_t count);

        void
        registerRetry(std::uint64_t count);

        void
        registerError(std::uint64_t count);

        boost::json::object
        report() const;

    private:
        std::string name_;
        std::atomic_uint64_t pendingCounter_ = 0u;
        std::atomic_uint64_t completedCounter_ = 0u;
        std::atomic_uint64_t retryCounter_ = 0u;
        std::atomic_uint64_t errorCounter_ = 0u;
    };

    std::atomic_uint64_t tooBusyCounter_ = 0u;

    std::atomic_uint64_t writeSyncCounter_ = 0u;
    std::atomic_uint64_t writeSyncRetryCounter_ = 0u;

    AsyncOperationCounters asyncWriteCounters_{"write_async"};
    AsyncOperationCounters asyncReadCounters_{"read_async"};
};

}  // namespace data
