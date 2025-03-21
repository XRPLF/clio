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

#pragma once

#include "cluster/ClioNode.hpp"
#include "cluster/ClusterCommunicationServiceInterface.hpp"
#include "data/BackendInterface.hpp"
#include "util/Assert.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <boost/asio/spawn.hpp>

#include <chrono>
#include <concepts>
#include <memory>
#include <vector>

namespace cluster {

class ClusterCommunicationService : public ClusterCommunicationServiceInterface {
    using ContextType = util::async::CoroExecutionContext;

    ContextType ctx_;
    mutable ContextType::Strand strand_ = ctx_.makeStrand();

    std::shared_ptr<data::BackendInterface> backend_;

    ContextType::Strand::RepeatedOperation readOperation_;
    ContextType::Strand::RepeatedOperation writeOperation_;

    ClioNode selfData_;
    std::vector<ClioNode> otherNodesData_;

public:
    ClusterCommunicationService(
        std::shared_ptr<data::BackendInterface> backend,
        std::chrono::steady_clock::duration readInterval,
        std::chrono::steady_clock::duration writeInterval
    );

    ~ClusterCommunicationService() override;

    ClusterCommunicationService(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService(ClusterCommunicationService const&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService const&) = delete;

    ClioNode
    selfData() const override;

    std::vector<ClioNode>
    clusterData() const override;

private:
    template <std::invocable Fn>
    auto
    executeOnStrand(Fn&& fn) const
    {
        auto operation = strand_.execute(std::forward<Fn>(fn));
        auto result = operation.get();
        ASSERT(result.has_value(), "Unexpected error in async operation");
        return std::move(result).value();
    }

    void
    doRead(ContextType::StopToken yield);

    void
    doWrite();
};

}  // namespace cluster
