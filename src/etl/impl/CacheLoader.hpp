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

#include "data/BackendInterface.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/impl/CursorProvider.hpp"
#include "util/Assert.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

namespace etl::impl {

struct CacheLoaderSettings {
    static constexpr size_t DEFAULT_NUM_CACHE_DIFFS = 32;
    static constexpr size_t DEFAULT_NUM_CACHE_MARKERS = 48;
    static constexpr size_t DEFAULT_CACHE_PAGE_FETCH_SIZE = 512;
    static constexpr size_t DEFAULT_NUM_THREADS = 2;

    enum class LoadStyle { ASYNC, SYNC, NOT_AT_ALL };

    size_t numCacheDiffs = DEFAULT_NUM_CACHE_DIFFS; /**< number of diffs to use to generate cursors */
    size_t numCacheMarkers =
        DEFAULT_NUM_CACHE_MARKERS; /**< number of markers to use at one time to traverse the ledger */
    size_t cachePageFetchSize =
        DEFAULT_CACHE_PAGE_FETCH_SIZE;       /**< number of ledger objects to fetch concurrently per marker */
    size_t numThreads = DEFAULT_NUM_THREADS; /**< number of threads to use for loading cache */

    LoadStyle loadStyle = LoadStyle::ASYNC;

    explicit CacheLoaderSettings(util::Config const& config)
    {
        numThreads = config.valueOr("io_threads", numThreads);
        if (config.contains("cache")) {
            auto const cache = config.section("cache");
            numCacheDiffs = cache.valueOr<size_t>("num_diffs", numCacheDiffs);
            numCacheMarkers = cache.valueOr<size_t>("num_markers", numCacheMarkers);
            cachePageFetchSize = cache.valueOr<size_t>("page_fetch_size", cachePageFetchSize);

            if (auto entry = cache.maybeValue<std::string>("load"); entry) {
                if (boost::iequals(*entry, "sync"))
                    loadStyle = LoadStyle::SYNC;
                if (boost::iequals(*entry, "async"))
                    loadStyle = LoadStyle::ASYNC;
                if (boost::iequals(*entry, "none") or boost::iequals(*entry, "no"))
                    loadStyle = LoadStyle::NOT_AT_ALL;
            }
        }
    }

    [[nodiscard]] bool
    isSync() const
    {
        return loadStyle == LoadStyle::SYNC;
    }

    [[nodiscard]] bool
    isAsync() const
    {
        return loadStyle == LoadStyle::ASYNC;
    }

    [[nodiscard]] bool
    isDisabled() const
    {
        return loadStyle == LoadStyle::NOT_AT_ALL;
    }
};

template <typename CacheType, typename ExecutionContextType = util::async::CoroExecutionContext>
class CacheLoaderImpl {
    util::Logger log_{"ETL"};

    CacheLoaderSettings settings_;
    ExecutionContextType ctx_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;

    using OpType = typename ExecutionContextType::template StoppableOperation<void>;
    etl::ThreadSafeQueue<CursorPair> queue_;

    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::vector<OpType> tasks_;
    std::atomic_int16_t remaining_;
    std::atomic_bool cancelled_ = false;

public:
    CacheLoaderImpl(
        CacheLoaderSettings settings,
        std::shared_ptr<BackendInterface> const& backend,
        CacheType& cache,
        uint32_t const seq,
        std::vector<CursorPair> const& cursors
    )
        : settings_{settings}
        , ctx_{settings_.numThreads}
        , backend_{backend}
        , cache_{cache}
        , queue_{cursors.size()}
        , remaining_{cursors.size()}
    {
        ASSERT(not cache_.get().isFull(), "Cache must not be full. seq = {}", seq);

        std::ranges::for_each(cursors, [this](auto const& cursor) { queue_.push(cursor); });
        load(seq);
    }

    void
    stop() noexcept
    {
        cancelled_ = true;
        for (auto& t : tasks_)
            t.requestStop();
    }

    void
    wait() noexcept
    {
        for (auto& t : tasks_)
            t.wait();
    }

private:
    void
    load(uint32_t const seq)
    {
        namespace vs = std::views;

        LOG(log_.info()) << "Loading cache. Num cursors = " << queue_.size();
        for ([[maybe_unused]] auto taskId : vs::iota(0u, settings_.numCacheMarkers))
            tasks_.push_back(spawnWorker(seq));

        if (settings_.isSync()) {
            for (auto& t : tasks_)
                t.wait();

            if (not cancelled_)
                ASSERT(cache_.get().isFull(), "Cache must be full");
        }

        if (cancelled_)
            LOG(log_.info()) << "Cache loading cancelled";
    }

    OpType
    spawnWorker(uint32_t const seq)
    {
        return ctx_.execute([this, seq](auto token) {
            while (not token.isStopRequested()) {
                auto cursor = queue_.tryPop();
                if (not cursor.has_value()) {
                    return;  // queue is empty
                }

                auto [start, end] = cursor.value();
                LOG(log_.debug()) << "Starting a cursor: " << ripple::strHex(start);

                while (not token.isStopRequested()) {
                    auto res = data::retryOnTimeout([this, seq, &start, token]() {
                        return backend_->fetchLedgerPage(start, seq, settings_.cachePageFetchSize, false, token);
                    });

                    cache_.get().update(res.objects, seq, true);

                    if (not res.cursor or res.cursor > end) {
                        if (--remaining_ <= 0) {
                            cache_.get().setFull();
                        }

                        LOG(log_.debug()) << "Finished a cursor. Remaining = " << remaining_;
                        break;  // pick up the next cursor if available
                    }

                    start = std::move(res.cursor).value();
                }
            }
        });
    }
};

}  // namespace etl::impl
