#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief Handles the `gateway_balances` command
 *
 * The gateway_balances command calculates the total balances issued by a given account, optionally
 * excluding amounts held by operational addresses.
 *
 * For more details see: https://xrpl.org/gateway_balances.html#gateway_balances
 */
class GatewayBalancesHandler
    : public rpc::spec::HandlerFor<rpc::spec::handlers::gateway_balances::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::string accountID;
        bool overflow = false;
        std::map<xrpl::Currency, xrpl::STAmount> sums;
        std::map<xrpl::AccountID, std::vector<xrpl::STAmount>> hotBalances;
        std::map<xrpl::AccountID, std::vector<xrpl::STAmount>> assets;
        std::map<xrpl::AccountID, std::vector<xrpl::STAmount>> frozenBalances;
        std::map<xrpl::Currency, xrpl::STAmount> locked;
        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new GatewayBalancesHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    GatewayBalancesHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the GatewayBalances command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);
};

}  // namespace rpc
