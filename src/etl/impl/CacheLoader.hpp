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
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace etl::impl {

template <typename CacheType>
class CacheLoaderImpl {
    util::Logger log_{"ETL"};

    util::async::AnyExecutionContext ctx_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;

    etl::ThreadSafeQueue<CursorPair> queue_;
    std::atomic_int16_t remaining_;

    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::vector<util::async::AnyOperation<void>> tasks_;

public:
    template <typename CtxType>
    CacheLoaderImpl(
        CtxType& ctx,
        std::shared_ptr<BackendInterface> const& backend,
        CacheType& cache,
        uint32_t const seq,
        std::size_t const numCacheMarkers,
        std::size_t const cachePageFetchSize,
        std::vector<CursorPair> const& cursors
    )
        : ctx_{ctx}, backend_{backend}, cache_{std::ref(cache)}, queue_{cursors.size()}, remaining_{cursors.size()}
    {
        std::ranges::for_each(cursors, [this](auto const& cursor) { queue_.push(cursor); });
        load(seq, numCacheMarkers, cachePageFetchSize);
    }

    ~CacheLoaderImpl()
    {
        stop();
        wait();
    }

    void
    stop() noexcept
    {
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
    load(uint32_t const seq, size_t numCacheMarkers, size_t cachePageFetchSize)
    {
        namespace vs = std::views;

        LOG(log_.info()) << "Loading cache. Num cursors = " << queue_.size();
        tasks_.reserve(numCacheMarkers);

        for ([[maybe_unused]] auto taskId : vs::iota(0u, numCacheMarkers))
            tasks_.push_back(spawnWorker(seq, cachePageFetchSize));
    }

    [[nodiscard]] auto
    spawnWorker(uint32_t const seq, size_t cachePageFetchSize)
    {
        return ctx_.execute([this, seq, cachePageFetchSize](auto token) {
            while (not token.isStopRequested()) {
                auto cursor = queue_.tryPop();
                if (not cursor.has_value()) {
                    return;  // queue is empty
                }

                auto [start, end] = cursor.value();
                LOG(log_.debug()) << "Starting a cursor: " << ripple::strHex(start);

                while (not token.isStopRequested()) {
                    auto res = data::retryOnTimeout([this, seq, cachePageFetchSize, &start, token]() {
                        return backend_->fetchLedgerPage(start, seq, cachePageFetchSize, false, token);
                    });

                    cache_.get().update(res.objects, seq, true);

                    if (not res.cursor or res.cursor > end) {
                        if (--remaining_ <= 0) {
                            auto endTime = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime_);

                            LOG(log_.info()) << "Finished loading cache. Cache size = " << cache_.get().size()
                                             << ". Took " << duration.count() << " seconds";

                            cache_.get().setFull();
                        } else {
                            LOG(log_.debug()) << "Finished a cursor. Remaining = " << remaining_;
                        }

                        break;  // pick up the next cursor if available
                    }

                    start = std::move(res.cursor).value();
                }
            }
        });
    }
};

}  // namespace etl::impl
