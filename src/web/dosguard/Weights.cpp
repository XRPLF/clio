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

#include "web/dosguard/Weights.hpp"

#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace web::dosguard {

Weights::Weights(size_t defaultWeight, std::unordered_map<std::string, size_t> weights)
    : defaultWeight_(defaultWeight), weights_(std::move(weights))
{
}

Weights
Weights::make(util::config::ClioConfigDefinition const& config)
{
    std::unordered_map<std::string, size_t> weights;
    auto const configWeights = config.getArray("dos_guard.__ng_weights");
    for (size_t i = 0; i < configWeights.size(); ++i) {
        auto const w = configWeights.objectAt(i);
        weights.emplace(w.get<std::string>("method"), w.get<size_t>("weight"));
    }
    return Weights{config.get<size_t>("dos_guard.__ng_default_weight"), std::move(weights)};
}

size_t
Weights::commandWeight(std::string const& cmd) const
{
    auto it = weights_.find(cmd);
    return it != weights_.end() ? it->second : defaultWeight_;
}

}  // namespace web::dosguard
