//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <data/BackendInterface.h>
#include <rpc/JS.h>

#include <optional>

namespace etl {

/**
 * @brief This class is responsible for fetching and storing the state of the ETL information, such as the network id
 *
 */
template <class LoadBalancerType>
class ETLState
{
    struct State
    {
        std::optional<uint32_t> networkID;
    };

    std::shared_ptr<LoadBalancerType> loadBalancer_;
    std::optional<State> state_;

public:
    ETLState(std::shared_ptr<LoadBalancerType> balancer) : loadBalancer_(std::move(balancer))
    {
    }

    /**
     * @brief Get the network id of ETL
     */
    std::optional<uint32_t>
    getNetworkID() const noexcept
    {
        if (!state_ || state_->networkID)
            return std::nullopt;

        return state_->networkID;
    }

    /**
     * @brief Fetch the ETL state from the rippled server
     *
     * @return true if the state was fetched successfully
     */
    bool
    fetchETLState()
    {
        auto const serverInfoRippled = data::synchronous([this](auto yield) {
            return loadBalancer_->forwardToRippled({{"command", "server_info"}}, std::nullopt, yield);
        });

        if (serverInfoRippled && !serverInfoRippled->contains(JS(error)))
        {
            state_.emplace(State());

            if (serverInfoRippled->contains(JS(result)) &&
                serverInfoRippled->at(JS(result)).as_object().contains(JS(info)))
            {
                auto const rippledInfo = serverInfoRippled->at(JS(result)).as_object().at(JS(info)).as_object();
                if (rippledInfo.contains(JS(network_id)))
                    state_->networkID.emplace(boost::json::value_to<int64_t>(rippledInfo.at(JS(network_id))));
            }
            return true;
        }
        return false;
    }
};

}  // namespace etl
