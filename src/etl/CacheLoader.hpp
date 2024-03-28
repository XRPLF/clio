//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
#include "etl/CacheLoaderSettings.hpp"
#include "etl/impl/CacheLoader.hpp"
#include "etl/impl/CursorFromAccountProvider.hpp"
#include "etl/impl/CursorFromDiffProvider.hpp"
#include "etl/impl/CursorFromFixDiffNumProvider.hpp"
#include "util/Assert.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <functional>
#include <memory>

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
template <typename CacheType, typename ExecutionContextType = util::async::CoroExecutionContext>
class CacheLoader {
    using CacheLoaderType = impl::CacheLoaderImpl<CacheType>;

    util::Logger log_{"ETL"};
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;

    CacheLoaderSettings settings_;
    ExecutionContextType ctx_;
    std::unique_ptr<CacheLoaderType> loader_{};

public:
    /**
     * @brief Construct a new Cache Loader object
     *
     * @param config The configuration to use
     * @param backend The backend to use
     * @param cache The cache to load into
     */
    CacheLoader(util::Config const& config, std::shared_ptr<BackendInterface> const& backend, CacheType& cache)
        : backend_{backend}, cache_{cache}, settings_{make_CacheLoaderSettings(config)}, ctx_{settings_.numThreads}
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
    load(uint32_t const seq)
    {
        ASSERT(not cache_.get().isFull(), "Cache must not be full. seq = {}", seq);

        if (settings_.isDisabled()) {
            cache_.get().setDisabled();
            LOG(log_.warn()) << "Cache is disabled. Not loading";
            return;
        }

        std::shared_ptr<impl::BaseCursorProvider> provider;
        if (settings_.numCacheCursorsFromDiff != 0) {
            LOG(log_.info()) << "Loading cache with cursor from num_cursors_from_diff="
                             << settings_.numCacheCursorsFromDiff;
            provider = std::make_shared<impl::CursorFromDiffProvider>(backend_, settings_.numCacheCursorsFromDiff);
        } else if (settings_.numCacheCursorsFromAccount != 0) {
            LOG(log_.info()) << "Loading cache with cursor from num_cursors_from_account="
                             << settings_.numCacheCursorsFromAccount;
            provider = std::make_shared<impl::CursorFromAccountProvider>(
                backend_, settings_.numCacheCursorsFromAccount, settings_.cachePageFetchSize
            );
        } else {
            LOG(log_.info()) << "Loading cache with cursor from num_diffs=" << settings_.numCacheDiffs;
            provider = std::make_shared<impl::CursorFromFixDiffNumProvider>(backend_, settings_.numCacheDiffs);
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
    stop() noexcept
    {
        loader_->stop();
    }

    /**
     * @brief Waits for the loader to finish background work
     */
    void
    wait() noexcept
    {
        loader_->wait();
    }
};

}  // namespace etl
