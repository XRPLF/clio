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

#include "util/Spawn.hpp"
#include "util/log/Logger.hpp"

#include <algorithm>
#include <utility>

namespace cluster {

WriterDecider::WriterDecider(boost::asio::thread_pool& ctx, std::unique_ptr<etl::WriterStateInterface> writerState)
    : ctx_(ctx), writerState_(std::move(writerState))
{
}

void
WriterDecider::onNewState(ClioNode::cUUID selfId, std::shared_ptr<Backend::ClusterData const> clusterData)
{
    util::spawn(
        ctx_,
        [writerState = writerState_->clone(),
         selfId = std::move(selfId),
         clusterData = std::move(clusterData)](auto&&) {
            if (not clusterData->has_value())
                return;
            auto data = clusterData->value();
            std::ranges::sort(data, [](ClioNode const& lhs, ClioNode const& rhs) { return lhs.uuid < rhs.uuid; });

            auto const it = std::ranges::find_if(data, [](ClioNode const& node) {
                return node.dbRole != ClioNode::DbRole::ReadOnly;
            });

            if (it == data.end()) {
                LOG(util::LogService::warn()) << "No nodes allowed to write in the cluster";
                return;
            }

            if (it->uuid == selfId) {
                writerState->startWriting();
            } else {
                writerState->giveUpWriting();
            }
        }
    );
}

}  // namespace cluster
