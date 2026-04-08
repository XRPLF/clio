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
