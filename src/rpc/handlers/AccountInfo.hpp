#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/account_info/Types.hpp>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief The account_info command retrieves information about an account, its activity, and its XRP
 * balance.
 *
 * For more details see: https://xrpl.org/account_info.html
 */
class AccountInfoHandler : public rpc::spec::HandlerFor<rpc::spec::handlers::account_info::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex;
        std::string ledgerHash;
        xrpl::STLedgerEntry accountData;
        bool isDisallowIncomingEnabled = false;
        bool isClawbackEnabled = false;
        bool isTokenEscrowEnabled = false;
        uint32_t apiVersion;
        std::optional<std::vector<xrpl::STLedgerEntry>> signerLists;
        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountInfoHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param amendmentCenter The amendment center to use
     */
    AccountInfoHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), amendmentCenter_{amendmentCenter}
    {
    }

    /**
     * @brief Process the AccountInfo command
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
