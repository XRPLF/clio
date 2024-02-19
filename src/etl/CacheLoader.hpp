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
#include "util/Assert.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace etl {

/**
 * @brief Cache loading interface
 */
template <
    typename CacheType,
    typename CursorProviderType = impl::CursorProvider,
    typename ExecutionContextType = util::async::CoroExecutionContext>
class CacheLoader {
    using CacheLoaderType = impl::CacheLoaderImpl<CacheType>;

    util::Logger log_{"ETL"};
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;

    CacheLoaderSettings settings_;
    ExecutionContextType ctx_;
    std::unique_ptr<CacheLoaderType> loader_;

public:
    CacheLoader(util::Config const& config, std::shared_ptr<BackendInterface> const& backend, CacheType& ledgerCache)
        : backend_{backend}
        , cache_{ledgerCache}
        , settings_{make_CacheLoaderSettings(config)}
        , ctx_{settings_.numThreads}
    {
    }

    void
    load(uint32_t const seq)
    {
        ASSERT(not cache_.get().isFull(), "Cache must not be full. seq = {}", seq);

        // TODO: move somewhere else, this is not part of load's responsibilities
        if (settings_.isDisabled()) {
            cache_.get().setDisabled();
            LOG(log_.warn()) << "Cache is disabled. Not loading";
            return;
        }

        auto const provider = CursorProviderType{backend_, settings_.numCacheDiffs};
        loader_ = std::make_unique<CacheLoaderType>(
            ctx_,
            backend_,
            cache_,
            seq,
            settings_.numCacheMarkers,
            settings_.cachePageFetchSize,
            provider.getCursors(seq)
        );

        if (settings_.isSync()) {
            loader_->wait();
            ASSERT(cache_.get().isFull(), "Cache must be full after sync load. seq = {}", seq);
        }
    }

    void
    stop() noexcept
    {
        loader_->stop();
    }

    void
    wait() noexcept
    {
        loader_->wait();
    }
};

}  // namespace etl
