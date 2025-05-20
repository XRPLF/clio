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

#include "util/StringHash.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "web/dosguard/WeightsInterface.hpp"

#include <boost/json/object.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace web::dosguard {

/**
 * @brief Implementation of WeightsInterface that manages command weights for DosGuard.
 *
 * This class provides a mechanism to assign different weights to API commands
 * for the purpose of DOS protection calculations. Commands can have specific weights,
 * or fall back to a default weight.
 */
class Weights : public WeightsInterface {
public:
    /**
     * @brief Structure representing weight configuration for a command.
     *
     * Contains the base weight and optional specialized weights for different ledger specifications.
     */
    struct Entry {
        size_t weight;
        std::optional<size_t> weightLedgerCurrent;
        std::optional<size_t> weightLedgerValidated;
    };

private:
    size_t defaultWeight_;
    std::unordered_map<std::string, Entry, util::StringHash, std::equal_to<>> weights_;

public:
    /**
     * @brief Construct a new Weights object
     *
     * @param defaultWeight The default weight to use when a command-specific weight is not defined
     * @param weights Map of command names to their specific weights
     */
    Weights(size_t defaultWeight, std::unordered_map<std::string, Entry> weights);

    /**
     * @brief Create a Weights object from configuration
     *
     * @param config The application configuration
     * @return Weights instance initialized with values from configuration
     */
    static Weights
    make(util::config::ClioConfigDefinition const& config);

    /**
     * @brief Get the weight assigned to a specific command
     *
     * @param request Json request
     * @return size_t The weight value (specific weight if defined, otherwise default weight)
     */
    size_t
    requestWeight(boost::json::object const& request) const override;
};

}  // namespace web::dosguard
