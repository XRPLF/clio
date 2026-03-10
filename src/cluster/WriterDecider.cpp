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

#include "cluster/WriterDecider.hpp"

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "etl/WriterState.hpp"
#include "util/Assert.hpp"
#include "util/Mutex.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace cluster {

namespace {

constexpr std::chrono::seconds kRECOVERY_TIME = std::chrono::seconds{3600};

void
startFallbackRecoveryTimer(
    WriterDecider::FallbackRecoveryTimerType fallbackRecoveryTimer,
    std::unique_ptr<etl::WriterStateInterface> writerState
)
{
    auto timer = fallbackRecoveryTimer->lock();
    timer->expires_after(kRECOVERY_TIME);
    timer->async_wait([writerState = std::move(writerState)](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        ASSERT(!ec, "Unexpected error {}: {}", ec.value(), ec.to_string());
        writerState->setFallbackRecovery(true);
    });
}

}  // namespace

WriterDecider::WriterDecider(
    boost::asio::thread_pool& ctx,
    std::unique_ptr<etl::WriterStateInterface> writerState
)
    : ctx_(ctx)
    , writerState_(std::move(writerState))
    , fallbackRecoveryTimer_(
          std::make_shared<util::Mutex<boost::asio::steady_timer>>(
              boost::asio::steady_timer(ctx.get_executor())
          )
      )
{
}

void
WriterDecider::onNewState(
    ClioNode::CUuid selfId,
    std::shared_ptr<Backend::ClusterData const> clusterData
)
{
    if (not clusterData->has_value())
        return;

    util::spawn(
        ctx_,
        [writerState = writerState_->clone(),
         selfId = std::move(selfId),
         fallbackRecoveryTimer = fallbackRecoveryTimer_,
         clusterData = clusterData->value()](auto&&) mutable {
            auto const selfData = std::ranges::find_if(
                clusterData, [&selfId](ClioNode const& node) { return node.uuid == selfId; }
            );
            ASSERT(selfData != clusterData.end(), "Self data should always be in the cluster data");

            if (selfData->dbRole == ClioNode::DbRole::ReadOnly) {
                writerState->giveUpWriting();
                return;
            }

            if (selfData->dbRole == ClioNode::DbRole::Fallback) {
                auto const clusterInFallbackRecoveryState =
                    std::ranges::any_of(clusterData, [](ClioNode const& node) {
                        return node.dbRole == ClioNode::DbRole::FallbackRecovery;
                    });
                if (clusterInFallbackRecoveryState) {
                    writerState->setFallbackRecovery(true);
                    fallbackRecoveryTimer->lock()->cancel();
                }
                return;
            }

            if (selfData->dbRole == ClioNode::DbRole::FallbackRecovery) {
                auto const clusterIsReadyToRecover =
                    not std::ranges::any_of(clusterData, [](ClioNode const& node) {
                        return node.dbRole == ClioNode::DbRole::Fallback;
                    });
                if (clusterIsReadyToRecover) {
                    writerState->giveUpWriting();
                    writerState->setFallbackRecovery(false);
                }
                return;
            }

            // If any node in the cluster is in Fallback mode, the entire cluster must switch
            // to the fallback writer decision mechanism for consistency
            auto const clusterInFallbackState =
                std::ranges::any_of(clusterData, [](ClioNode const& node) {
                    return node.dbRole == ClioNode::DbRole::Fallback;
                });
            if (clusterInFallbackState) {
                writerState->setWriterDecidingFallback();
                startFallbackRecoveryTimer(
                    std::move(fallbackRecoveryTimer), std::move(writerState)
                );
                return;
            }

            // We are not ReadOnly and there is no Fallback in the cluster
            // Election mode
            std::ranges::sort(clusterData, [](ClioNode const& lhs, ClioNode const& rhs) {
                return *lhs.uuid < *rhs.uuid;
            });

            auto it = std::ranges::find_if(clusterData, [](ClioNode const& node) {
                return node.etlStarted and node.cacheIsFull and
                    (node.dbRole == ClioNode::DbRole::NotWriter or
                     node.dbRole == ClioNode::DbRole::Writer);
            });

            auto electNode = [&selfId, &writerState](auto it) {
                if (*it->uuid == *selfId) {
                    writerState->startWriting();
                } else {
                    writerState->giveUpWriting();
                }
            };
            if (it != clusterData.end()) {
                electNode(it);
                return;
            }

            // Try to find a node with at least started ETL
            it = std::ranges::find_if(clusterData, [](ClioNode const& node) {
                return node.etlStarted and
                    (node.dbRole == ClioNode::DbRole::NotWriter or
                     node.dbRole == ClioNode::DbRole::Writer);
            });

            if (it != clusterData.end()) {
                electNode(it);
                return;
            }
            writerState->giveUpWriting();
        }
    );
}

}  // namespace cluster
