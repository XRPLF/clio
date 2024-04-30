//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"

#include <boost/json/object.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace data {

/**
 * @brief A concept for a class that can be used to count backend operations.
 */
template <typename T>
concept SomeBackendCounters = requires(T a) {
    typename T::PtrType;
    { a.registerTooBusy() } -> std::same_as<void>;
    { a.registerWriteSync(std::chrono::steady_clock::time_point{}) } -> std::same_as<void>;
    { a.registerWriteSyncRetry() } -> std::same_as<void>;
    { a.registerWriteStarted() } -> std::same_as<void>;
    { a.registerWriteFinished(std::chrono::steady_clock::time_point{}) } -> std::same_as<void>;
    { a.registerWriteRetry() } -> std::same_as<void>;
    { a.registerReadStarted(std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadFinished(std::chrono::steady_clock::time_point{}, std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadRetry(std::uint64_t{}) } -> std::same_as<void>;
    { a.registerReadError(std::uint64_t{}) } -> std::same_as<void>;
    { a.report() } -> std::same_as<boost::json::object>;
};

/**
 * @brief Holds statistics about the backend.
 *
 * @note This class is thread-safe.
 */
class BackendCounters {
public:
    using PtrType = std::shared_ptr<BackendCounters>;

    /**
     * @brief Create a new BackendCounters object
     *
     * @return A shared pointer to the new BackendCounters object
     */
    static PtrType
    make();

    /**
     * @brief Register that the backend was too busy to process a request
     */
    void
    registerTooBusy();

    /**
     * @brief Register that a write operation was started
     *
     * @param startTime The time the operation was started
     */
    void
    registerWriteSync(std::chrono::steady_clock::time_point startTime);

    /**
     * @brief Register that a write operation was retried
     */
    void
    registerWriteSyncRetry();

    /**
     * @brief Register that a write operation was started
     */
    void
    registerWriteStarted();

    /**
     * @brief Register that a write operation was finished
     *
     * @param startTime The time the operation was started
     */
    void
    registerWriteFinished(std::chrono::steady_clock::time_point startTime);

    /**
     * @brief Register that a write operation was retried
     */
    void
    registerWriteRetry();

    /**
     * @brief Register that one or more read operations were started
     *
     * @param count The number of operations started
     */
    void
    registerReadStarted(std::uint64_t count = 1u);

    /**
     * @brief Register that one or more read operations were finished
     *
     * @param startTime The time the operations were started
     * @param count The number of operations finished
     */
    void
    registerReadFinished(std::chrono::steady_clock::time_point startTime, std::uint64_t count = 1u);

    /**
     * @brief Register that one or more read operations were retried
     *
     * @param count The number of operations retried
     */
    void
    registerReadRetry(std::uint64_t count = 1u);

    /**
     * @brief Register that one or more read operations had an error
     *
     * @param count The number of operations with an error
     */
    void
    registerReadError(std::uint64_t count = 1u);

    /**
     * @brief Get a report of the backend counters
     *
     * @return The report
     */
    boost::json::object
    report() const;

private:
    BackendCounters();

    class AsyncOperationCounters {
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
        std::reference_wrapper<util::prometheus::GaugeInt> pendingCounter_;
        std::reference_wrapper<util::prometheus::CounterInt> completedCounter_;
        std::reference_wrapper<util::prometheus::CounterInt> retryCounter_;
        std::reference_wrapper<util::prometheus::CounterInt> errorCounter_;
    };

    std::reference_wrapper<util::prometheus::CounterInt> tooBusyCounter_;

    std::reference_wrapper<util::prometheus::CounterInt> writeSyncCounter_;
    std::reference_wrapper<util::prometheus::CounterInt> writeSyncRetryCounter_;

    AsyncOperationCounters asyncWriteCounters_{"write_async"};
    AsyncOperationCounters asyncReadCounters_{"read_async"};

    std::reference_wrapper<util::prometheus::HistogramInt> readDurationHistogram_;
    std::reference_wrapper<util::prometheus::HistogramInt> writeDurationHistogram_;
};

}  // namespace data
