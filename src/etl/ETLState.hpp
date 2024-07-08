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

#include "data/BackendInterface.hpp"

#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>

#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief This class is responsible for fetching and storing the state of the ETL information, such as the network id
 */
struct ETLState {
    std::optional<uint32_t> networkID;

    /**
     * @brief Fetch the ETL state from the rippled server
     * @param source The source to fetch the state from
     * @return The ETL state, nullopt if source not available
     */
    template <typename Forward>
    static std::optional<ETLState>
    fetchETLStateFromSource(Forward& source) noexcept
    {
        auto const serverInfoRippled = data::synchronous([&source](auto yield) -> std::optional<boost::json::object> {
            if (auto result = source.forwardToRippled({{"command", "server_info"}}, std::nullopt, {}, yield)) {
                return std::move(result).value();
            }
            return std::nullopt;
        });

        if (serverInfoRippled)
            return boost::json::value_to<std::optional<ETLState>>(boost::json::value(*serverInfoRippled));

        return std::nullopt;
    }
};

/**
 * @brief Parse a boost::json::value into a ETLState
 *
 * @param jv The json value to convert
 * @return The ETLState
 */
std::optional<ETLState>
tag_invoke(boost::json::value_to_tag<std::optional<ETLState>>, boost::json::value const& jv);

}  // namespace etl
