#pragma once

#include "data/BackendInterface.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "data/Types.hpp"
#include "etl/CacheLoaderInterface.hpp"
#include "etl/CacheLoaderSettings.hpp"
#include "etl/impl/CacheLoader.hpp"
#include "etl/impl/CursorFromAccountProvider.hpp"
#include "etl/impl/CursorFromDiffProvider.hpp"
#include "etl/impl/CursorFromFixDiffNumProvider.hpp"
#include "util/Assert.hpp"
#include "util/Profiler.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace etl {

/**
 * @brief Cache loading interface
 *
 * This class is responsible for loading the cache for a given sequence number.
 *
 * @tparam CacheType The type of the cache to load
 * @tparam CursorProviderType The type of the cursor provider to use
 * @tparam ExecutionContextType The type of the execution context to use
 */
template <typename ExecutionContextType = util::async::CoroExecutionContext>
class CacheLoader : public CacheLoaderInterface {
    using CacheLoaderType = impl::CacheLoaderImpl<data::LedgerCacheInterface>;

    util::Logger log_{"ETL"};
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<data::LedgerCacheInterface> cache_;

    CacheLoaderSettings settings_;
    std::unique_ptr<data::LedgerCacheLoadingStateInterface const> cacheLoadingState_;
    ExecutionContextType ctx_;
    std::unique_ptr<CacheLoaderType> loader_;

public:
    /**
     * @brief Construct a new Cache Loader object
     *
     * @param config The configuration to use
     * @param backend The backend to use
     * @param cache The cache to load into
     * @param cacheLoadingState State controlling whether loading from backend is currently allowed
     */
    CacheLoader(
        util::config::ClioConfigDefinition const& config,
        std::shared_ptr<BackendInterface> backend,
        data::LedgerCacheInterface& cache,
        std::unique_ptr<data::LedgerCacheLoadingStateInterface const> cacheLoadingState
    )
        : backend_{std::move(backend)}
        , cache_{cache}
        , settings_{makeCacheLoaderSettings(config)}
        , cacheLoadingState_(std::move(cacheLoadingState))
        , ctx_{settings_.numThreads}
    {
    }

    /**
     * @brief Load the cache for the given sequence number
     *
     * This function is blocking if the cache load style is set to sync and
     * disables the cache entirely if the load style is set to none/no.
     *
     * @param seq The sequence number to load cache for
     */
    void
    load(uint32_t const seq) override
    {
        ASSERT(not cache_.get().isFull(), "Cache must not be full. seq = {}", seq);

        if (settings_.isDisabled()) {
            cache_.get().setDisabled();
            LOG(log_.warn()) << "Cache is disabled. Not loading";
            return;
        }

        if (loadCacheFromFile()) {
            // Cache file may contain outdated data, so fetch whatever left up to seq from DB
            updateCacheToSeq(seq);
            cache_.get().setFull();
            return;
        }

        LOG(log_.info()) << "Waiting for ledger cache loading to become allowed";
        cacheLoadingState_->waitForLoadingAllowed();
        LOG(log_.info()) << "Ledger cache loading is now allowed. Start loading...";
        cache_.get().startLoading();

        std::shared_ptr<impl::BaseCursorProvider> provider;
        if (settings_.numCacheCursorsFromDiff != 0) {
            LOG(log_.info()) << "Loading cache with cursor from num_cursors_from_diff="
                             << settings_.numCacheCursorsFromDiff;
            provider = std::make_shared<impl::CursorFromDiffProvider>(
                backend_, settings_.numCacheCursorsFromDiff
            );
        } else if (settings_.numCacheCursorsFromAccount != 0) {
            LOG(log_.info()) << "Loading cache with cursor from num_cursors_from_account="
                             << settings_.numCacheCursorsFromAccount;
            provider = std::make_shared<impl::CursorFromAccountProvider>(
                backend_, settings_.numCacheCursorsFromAccount, settings_.cachePageFetchSize
            );
        } else {
            LOG(log_.info()) << "Loading cache with cursor from num_diffs="
                             << settings_.numCacheDiffs;
            provider = std::make_shared<impl::CursorFromFixDiffNumProvider>(
                backend_, settings_.numCacheDiffs
            );
        }

        loader_ = std::make_unique<CacheLoaderType>(
            ctx_,
            backend_,
            cache_,
            seq,
            settings_.numCacheMarkers,
            settings_.cachePageFetchSize,
            provider->getCursors(seq)
        );

        if (settings_.isSync()) {
            loader_->wait();
            ASSERT(cache_.get().isFull(), "Cache must be full after sync load. seq = {}", seq);
        }
    }

    /**
     * @brief Requests the loader to stop asap
     */
    void
    stop() noexcept override
    {
        if (loader_ != nullptr)
            loader_->stop();
    }

    /**
     * @brief Waits for the loader to finish background work
     */
    void
    wait() noexcept override
    {
        if (loader_ != nullptr)
            loader_->wait();
    }

private:
    bool
    loadCacheFromFile()
    {
        if (not settings_.cacheFileSettings.has_value()) {
            return false;
        }
        LOG(log_.info()) << "Loading ledger cache from " << settings_.cacheFileSettings->path;
        auto const minLatestSequence =
            backend_->fetchLedgerRange()
                .transform([this](data::LedgerRange const& range) {
                    return std::max(
                        range.maxSequence - settings_.cacheFileSettings->maxAge, range.minSequence
                    );
                })
                .value_or(0);

        auto const [success, duration_ms] = util::timed([&]() {
            return cache_.get().loadFromFile(settings_.cacheFileSettings->path, minLatestSequence);
        });

        if (not success.has_value()) {
            LOG(log_.warn()) << "Error loading cache from file: " << success.error();
            return false;
        }

        LOG(log_.info()) << "Loaded cache from file in " << duration_ms
                         << " ms. Latest sequence: " << cache_.get().latestLedgerSequence();
        return true;
    }

    void
    updateCacheToSeq(uint32_t const seq)
    {
        while (cache_.get().latestLedgerSequence() < seq) {
            auto const seqToLoad = cache_.get().latestLedgerSequence() + 1;
            LOG(log_.info()) << "Fetching ledger " << seqToLoad
                             << "from DB after loading cache from file";
            auto const diff = data::synchronousAndRetryOnTimeout([this, seqToLoad](auto yield) {
                return backend_->fetchLedgerDiff(seqToLoad, yield);
            });
            cache_.get().update(diff, seqToLoad);
            LOG(log_.info()) << "Updated cache to " << seqToLoad;
        }
    }
};

}  // namespace etl
