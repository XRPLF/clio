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

#include "rpc/JS.hpp"
#include "util/Assert.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/json/object.hpp>
#include <xrpl/protocol/jss.h>

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace web::dosguard {

Weights::Weights(size_t defaultWeight, std::unordered_map<std::string, Entry> weights)
    : defaultWeight_(defaultWeight), weights_(std::move_iterator(weights.begin()), std::move_iterator(weights.end()))
{
}

Weights
Weights::make(util::config::ClioConfigDefinition const& config)
{
    std::unordered_map<std::string, Weights::Entry> weights;
    auto const configWeights = config.getArray("dos_guard.__ng_weights");
    for (size_t i = 0; i < configWeights.size(); ++i) {
        auto const w = configWeights.objectAt(i);
        Weights::Entry const entry{
            .weight = w.get<size_t>("weight"),
            .weightLedgerCurrent = w.maybeValue<size_t>("weight_ledger_current"),
            .weightLedgerValidated = w.maybeValue<size_t>("weight_ledger_validated"),
        };
        weights.emplace(w.get<std::string>("method"), entry);
    }
    return Weights{config.get<size_t>("dos_guard.__ng_default_weight"), std::move(weights)};
}

size_t
Weights::requestWeight(boost::json::object const& request) const
{
    if (not((request.contains(JS(method)) and request.at(JS(method)).is_string()) or
            (request.contains(JS(command)) and request.at(JS(command)).is_string()))) {
        return defaultWeight_;
    }

    std::string_view const cmd =
        request.contains(JS(method)) ? request.at(JS(method)).as_string() : request.at(JS(command)).as_string();

    auto it = weights_.find(cmd);
    if (it == weights_.end()) {
        return defaultWeight_;
    }

    auto const& entry = it->second;

    boost::json::value const* ledgerIndex = nullptr;
    if (request.contains(JS(ledger_index))) {
        ledgerIndex = &request.at(JS(ledger_index));
    } else if (request.contains(JS(params))) {
        ASSERT(
            request.at(JS(params)).is_array() and not request.at(JS(params)).as_array().empty() and
                request.at(JS(params)).as_array().at(0).is_object(),
            "params should be [{{}}]"
        );
        if (auto const& params = request.at(JS(params)).as_array().at(0).as_object();
            params.contains(JS(ledger_index))) {
            ledgerIndex = &params.at(JS(ledger_index));
        }
    }

    if (ledgerIndex != nullptr and ledgerIndex->is_string()) {
        auto const& ledgerIndexString = ledgerIndex->as_string();
        if (ledgerIndexString == JS(validated)) {
            return entry.weightLedgerValidated.value_or(entry.weight);
        }
        if (ledgerIndexString == JS(current)) {
            return entry.weightLedgerCurrent.value_or(entry.weight);
        }
    }
    return entry.weight;
}

}  // namespace web::dosguard
