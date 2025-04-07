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

#include <vector>

namespace cluster {

/**
 * @brief Interface for the cluster communication service.
 */
class ClusterCommunicationServiceInterface {
public:
    virtual ~ClusterCommunicationServiceInterface() = default;

    /**
     * @brief Get the data of the current node.
     *
     * @return The data of the current node.
     */
    [[nodiscard]] virtual ClioNode
    selfData() const = 0;

    /**
     * @brief Get the data of all nodes in the cluster (including self).
     *
     * @return The data of all nodes in the cluster.
     */
    [[nodiscard]] virtual std::vector<ClioNode>
    clusterData() const = 0;
};

}  // namespace cluster
