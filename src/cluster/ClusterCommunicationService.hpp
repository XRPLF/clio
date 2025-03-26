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
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace cluster {

class ClusterCommunicationService : public ClusterCommunicationServiceInterface {
    // TODO: Use util::async::CoroExecutionContext after https://github.com/XRPLF/clio/issues/1973 is implemented
    boost::asio::thread_pool ctx_{1};
    boost::asio::strand<boost::asio::thread_pool::executor_type> strand_ = boost::asio::make_strand(ctx_);

    util::Logger log_{"ClusterCommunication"};

    std::shared_ptr<data::BackendInterface> backend_;

    std::chrono::steady_clock::duration readInterval_;
    std::chrono::steady_clock::duration writeInterval_;

    ClioNode selfData_;
    std::vector<ClioNode> otherNodesData_;

    bool stopped_ = false;

public:
    ClusterCommunicationService(
        std::shared_ptr<data::BackendInterface> backend,
        std::chrono::steady_clock::duration readInterval,
        std::chrono::steady_clock::duration writeInterval
    );

    ~ClusterCommunicationService() override;

    void
    run();

    void
    stop();

    ClusterCommunicationService(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService(ClusterCommunicationService const&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService&&) = delete;
    ClusterCommunicationService&
    operator=(ClusterCommunicationService const&) = delete;

    std::shared_ptr<boost::uuids::uuid>
    selfUuid() const;

    ClioNode
    selfData() const override;

    std::vector<ClioNode>
    clusterData() const override;

private:
    void
    doRead(boost::asio::yield_context yield);

    void
    doWrite();
};

}  // namespace cluster
