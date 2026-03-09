//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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

#include "cluster/CacheLoaderDecider.hpp"

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "util/Assert.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace cluster {

CacheLoaderDecider::CacheLoaderDecider(
    boost::asio::thread_pool& ctx,
    std::unique_ptr<data::LedgerCacheLoadingStateInterface> cacheLoadingState
)
    : ctx_(ctx), cacheLoadingState_(std::move(cacheLoadingState))
{
}

void
CacheLoaderDecider::onNewState(
    ClioNode::CUuid selfId,
    std::shared_ptr<Backend::ClusterData const> clusterData
)
{
    if (not clusterData->has_value())
        return;

    util::spawn(
        ctx_,
        [cacheLoadingState = cacheLoadingState_->clone(),
         selfId = std::move(selfId),
         clusterData = clusterData->value()](auto&&) mutable {
            auto const selfData = std::ranges::find_if(
                clusterData, [&selfId](ClioNode const& node) { return node.uuid == selfId; }
            );
            ASSERT(selfData != clusterData.end(), "Self data should always be in the cluster data");

            if (selfData->cacheIsFull)
                return;

            std::vector<ClioNode> notFullNodes;
            std::ranges::copy_if(
                clusterData, std::back_inserter(notFullNodes), [](ClioNode const& node) {
                    return not node.cacheIsFull;
                }
            );

            auto const someNodeIsLoadingCache = std::ranges::any_of(
                notFullNodes, [](ClioNode const& node) { return node.cacheIsCurrentlyLoading; }
            );
            if (someNodeIsLoadingCache) {
                return;
            }

            std::ranges::sort(notFullNodes, [](ClioNode const& lhs, ClioNode const& rhs) {
                return *lhs.uuid < *rhs.uuid;
            });

            if (*notFullNodes.front().uuid == *selfId) {
                cacheLoadingState->allowLoading();
            }
        }
    );
}

}  // namespace cluster
