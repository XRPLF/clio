//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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

#include "etl/ETLHelpers.hpp"
#include "util/Assert.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <boost/asio/spawn.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <vector>

namespace migration::cassandra::impl {

/**
 * @brief The token range used to split the full table scan into multiple ranges.
 */
struct TokenRange {
    std::int64_t start;
    std::int64_t end;

    /**
     * @brief Construct a new Token Range object
     *
     * @param start The start token
     * @param end The end token
     */
    TokenRange(std::int64_t start, std::int64_t end) : start{start}, end{end}
    {
    }
};

/**
 * @brief The concept for an adapter.
 */
template <typename T>
concept CanReadByTokenRange = requires(T obj, TokenRange const& range, boost::asio::yield_context yield) {
    { obj.readByTokenRange(range, yield) } -> std::same_as<void>;
};

/**
 * @brief The full table scanner. It will split the full table scan into multiple ranges and read the data in given
 * executor.
 *
 * @tparam TableAdapter The table adapter type
 */
template <CanReadByTokenRange TableAdapter>
class FullTableScanner {
    /**
     * @brief The helper to generate the token ranges.
     */
    struct TokenRangesProvider {
        uint32_t numRanges;

        TokenRangesProvider(uint32_t numRanges) : numRanges{numRanges}
        {
        }

        [[nodiscard]] std::vector<TokenRange>
        getRanges() const
        {
            auto const minValue = std::numeric_limits<std::int64_t>::min();
            auto const maxValue = std::numeric_limits<std::int64_t>::max();
            if (numRanges == 1)
                return {TokenRange{minValue, maxValue}};

            // Safely calculate the range size using uint64_t to avoid overflow
            uint64_t const rangeSize = (static_cast<uint64_t>(maxValue) * 2) / numRanges;

            std::vector<TokenRange> ranges;
            ranges.reserve(numRanges);

            for (std::int64_t i = 0; i < numRanges; ++i) {
                int64_t const start = minValue + (i * rangeSize);
                int64_t const end = (i == numRanges - 1) ? maxValue : start + static_cast<int64_t>(rangeSize) - 1;
                ranges.emplace_back(start, end);
            }

            return ranges;
        }
    };

    [[nodiscard]] auto
    spawnWorker()
    {
        return ctx_.execute([this](auto token) {
            while (not token.isStopRequested()) {
                auto cursor = queue_.tryPop();
                if (not cursor.has_value()) {
                    return;  // queue is empty
                }
                reader_.readByTokenRange(cursor.value(), token);
            }
        });
    }

    void
    load(size_t workerNum)
    {
        namespace vs = std::views;

        tasks_.reserve(workerNum);

        for ([[maybe_unused]] auto taskId : vs::iota(0u, workerNum))
            tasks_.push_back(spawnWorker());
    }

    util::async::AnyExecutionContext ctx_;
    std::size_t cursorsNum_;
    etl::ThreadSafeQueue<TokenRange> queue_;
    std::vector<util::async::AnyOperation<void>> tasks_;
    TableAdapter reader_;

public:
    /**
     * @brief The full table scanner settings.
     */
    struct FullTableScannerSettings {
        std::uint32_t ctxThreadsNum; /**< number of threads used in the execution context */
        std::uint32_t jobsNum;       /**< number of coroutines to run, it is the number of concurrent database reads */
        std::uint32_t cursorsPerJob; /**< number of cursors per coroutine */
    };

    /**
     * @brief Construct a new Full Table Scanner object, it will run in a sync or async context according to the
     * parameter. The scan process will immediately start.
     *
     * @tparam ExecutionContextType The execution context type
     * @param settings The full table scanner settings
     * @param reader The table adapter
     */
    template <typename ExecutionContextType = util::async::CoroExecutionContext>
    FullTableScanner(FullTableScannerSettings settings, TableAdapter&& reader)
        : ctx_(ExecutionContextType(settings.ctxThreadsNum))
        , cursorsNum_(settings.jobsNum * settings.cursorsPerJob)
        , queue_{cursorsNum_}
        , reader_{std::move(reader)}
    {
        ASSERT(settings.jobsNum > 0, "jobsNum for full table scanner must be greater than 0");
        ASSERT(settings.cursorsPerJob > 0, "cursorsPerJob for full table scanner must be greater than 0");

        auto const cursors = TokenRangesProvider{cursorsNum_}.getRanges();
        std::ranges::for_each(cursors, [this](auto const& cursor) { queue_.push(cursor); });
        load(settings.jobsNum);
    }

    /**
     * @brief Wait for all workers to finish.
     */
    void
    wait()
    {
        for (auto& task : tasks_) {
            task.wait();
        }
    }
};

}  // namespace migration::cassandra::impl
