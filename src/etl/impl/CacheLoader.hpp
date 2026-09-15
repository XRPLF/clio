#pragma once

#include "data/BackendInterface.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/impl/BaseCursorProvider.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/context/detail/config.hpp>
#include <fmt/format.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

struct CacheLoaderImplTests;

namespace etl::impl {

template <typename CacheType>
class CacheLoaderImpl {
    util::Logger log_{"ETL"};

    util::async::AnyExecutionContext ctx_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;

    ThreadSafeQueue<CursorPair> queue_;
    std::atomic_int16_t remaining_;

    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::vector<util::async::AnyOperation<void>> tasks_;

public:
    template <typename CtxType>
    CacheLoaderImpl(
        CtxType& ctx,
        std::shared_ptr<BackendInterface> backend,
        CacheType& cache,
        uint32_t const seq,
        std::size_t const numCacheMarkers,
        std::size_t const cachePageFetchSize,
        std::vector<CursorPair> const& cursors
    )
        : ctx_{ctx}
        , backend_{std::move(backend)}
        , cache_{std::ref(cache)}
        , queue_{cursors.size()}
        , remaining_{cursors.size()}
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
            t.abort();
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
            runGuarded([this, seq, cachePageFetchSize, token] {
                loadCacheFromCursors(token, seq, cachePageFetchSize);
            });
        });
    }

    template <typename Work>
    void
    runGuarded(Work&& work)
    {
        std::optional<std::string> failure;
        try {
            std::forward<Work>(work)();
        } catch (std::exception const& e) {
            failure = fmt::format("Cache loading failed: {}", e.what());
        } catch (...) {
            failure = "Cache loading failed with an unknown (non-std) error";
        }

        if (failure.has_value()) {
            LOG(log_.error()) << *failure
                              << "; disabling cache and continuing without it (reads will be "
                                 "served from the database).";
            cache_.get().setDisabled();
        }
    }

    template <typename TokenType>
    void
    loadCacheFromCursors(TokenType token, uint32_t const seq, size_t cachePageFetchSize)
    {
        while (not token.isStopRequested() and not cache_.get().isDisabled()) {
            auto cursor = queue_.tryPop();
            if (not cursor.has_value()) {
                return;  // queue is empty
            }

            auto [start, end] = *cursor;
            LOG(log_.debug()) << "Starting a cursor: " << xrpl::strHex(start);

            while (not token.isStopRequested() and not cache_.get().isDisabled()) {
                auto res = data::retryOnTimeout(
                    [this, seq, cachePageFetchSize, &start, token]() {
                        return backend_->fetchLedgerPage(
                            start, seq, cachePageFetchSize, false, token
                        );
                    },
                    token,
                    util::Retry::Delays{
                        .initial = backend_->initialRetryDelay(), .max = backend_->maxRetryDelay()
                    }
                );

                cache_.get().update(res.objects, seq, true);

                if (not res.cursor or res.cursor > end) {
                    if (--remaining_ <= 0) {
                        auto endTime = std::chrono::steady_clock::now();
                        auto duration =
                            std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime_);

                        LOG(log_.info())
                            << "Finished loading cache. Cache size = " << cache_.get().size()
                            << ". Took " << duration.count() << " seconds";

                        cache_.get().setFull();
                    } else {
                        LOG(log_.debug()) << "Finished a cursor. Remaining = " << remaining_;
                    }

                    break;  // pick up the next cursor if available
                }

                start = *std::move(res.cursor);
            }
        }
    }

    // Grants tests access to the private guard/loading members above.
    friend struct ::CacheLoaderImplTests;
};

}  // namespace etl::impl
