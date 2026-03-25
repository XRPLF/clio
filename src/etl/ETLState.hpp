#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/JS.hpp"

#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief This class is responsible for fetching and storing the state of the ETL information, such
 * as the network id
 */
struct ETLState {
    /*
     * NOTE: Rippled NetworkID: Mainnet = 0; Testnet = 1; Devnet = 2
     * However, if rippled is running on neither of these (ie. standalone mode) rippled will default
     * to 0, but is not included in the stateOpt response. Must manually add it here.
     */
    uint32_t networkID{0};

    /**
     * @brief Fetch the ETL state from the rippled server
     * @param source The source to fetch the state from
     * @return The ETL state, nullopt if source not available
     */
    template <typename Forward>
    static std::optional<ETLState>
    fetchETLStateFromSource(Forward& source) noexcept
    {
        auto const serverInfoRippled =
            data::synchronous([&source](auto yield) -> std::optional<boost::json::object> {
                if (auto result = source.forwardToRippled(
                        {{"command", "server_info"}}, std::nullopt, {}, yield
                    )) {
                    return std::move(result).value();
                }
                return std::nullopt;
            });

        if (serverInfoRippled && not serverInfoRippled->contains(JS(error))) {
            return boost::json::value_to<ETLState>(boost::json::value(*serverInfoRippled));
        }

        return std::nullopt;
    }
};

/**
 * @brief Parse a boost::json::value into a ETLState
 *
 * @param jv The json value to convert
 * @return The ETLState
 */
ETLState
tag_invoke(boost::json::value_to_tag<ETLState>, boost::json::value const& jv);

}  // namespace etl
