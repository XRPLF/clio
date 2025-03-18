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

#include "data/BackendInterface.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace cluster {

struct ClioNode {
    // enum class WriterRole {
    //     ReadOnly,
    //     NotWriter,
    //     Writer
    // };
    boost::uuids::uuid uuid;
    std::chrono::system_clock::time_point updateTime;
    bool isSelf;
    // WriterRole writerRole;
};

class ClusterCommunicationService {
    using ContextType = util::async::CoroExecutionContext;
    ContextType ctx_;
    ContextType::Strand strand_ = ctx_.makeStrand();
    std::shared_ptr<data::BackendInterface> backend_;
    ContextType::RepeatedOperation readOperation_;
    ContextType::RepeatedOperation writeOperation_;
    ClioNode selfData_;
    std::vector<ClioNode> otherNodesData_;

public:
    ClusterCommunicationService(
        std::shared_ptr<data::BackendInterface> backend,
        std::chrono::steady_clock::duration readInterval,
        std::chrono::steady_clock::duration writeInterval
    );

    std::vector<ClioNode>
    clusterData() const;
};

}  // namespace cluster
