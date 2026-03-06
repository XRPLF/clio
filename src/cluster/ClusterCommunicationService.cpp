//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "cluster/ClusterCommunicationService.hpp"

#include "data/BackendInterface.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "etl/SystemState.hpp"
#include "etl/WriterState.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <chrono>
#include <ctime>
#include <memory>
#include <utility>

namespace cluster {

ClusterCommunicationService::ClusterCommunicationService(
    std::shared_ptr<data::BackendInterface> backend,
    std::unique_ptr<etl::WriterStateInterface> writerState,
    std::unique_ptr<data::LedgerCacheLoadingStateInterface> cacheLoadingState,
    std::chrono::steady_clock::duration readInterval,
    std::chrono::steady_clock::duration writeInterval
)
    : backend_(
          ctx_,
          std::move(backend),
          writerState->clone(),
          cacheLoadingState->clone(),
          readInterval,
          writeInterval
      )
    , writerDecider_(ctx_, std::move(writerState))
    , cacheLoaderDecider_(ctx_, std::move(cacheLoadingState))
{
}

void
ClusterCommunicationService::run()
{
    backend_.subscribeToNewState([this](auto&&... args) {
        metrics_.onNewState(std::forward<decltype(args)>(args)...);
    });
    backend_.subscribeToNewState([this](auto&&... args) {
        writerDecider_.onNewState(std::forward<decltype(args)>(args)...);
    });
    backend_.subscribeToNewState([this](auto&&... args) {
        cacheLoaderDecider_.onNewState(std::forward<decltype(args)>(args)...);
    });
    backend_.run();
}

ClusterCommunicationService::~ClusterCommunicationService()
{
    stop();
}

void
ClusterCommunicationService::stop()
{
    backend_.stop();
}

ClusterCommunicationService::MakeResult
ClusterCommunicationService::make(
    util::config::ClioConfigDefinition const& config,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<etl::SystemState> state
)
{
    auto const& cache = backend->cache();
    auto cacheLoadingState = std::make_unique<data::LedgerCacheLoadingState>(cache);
    if (not config.get<bool>("cache.limit_load_in_cluster")) {
        cacheLoadingState->allowLoading();
    }
    auto cacheLoadingStateClone = cacheLoadingState->clone();
    return MakeResult{
        .service = std::make_unique<ClusterCommunicationService>(
            std::move(backend),
            std::make_unique<etl::WriterState>(std::move(state), cache),
            std::move(cacheLoadingState)
        ),
        .cacheLoadingState = std::move(cacheLoadingStateClone)
    };
}

}  // namespace cluster
