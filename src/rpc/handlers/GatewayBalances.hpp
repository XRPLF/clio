#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/AccountUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/tokens.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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
class GatewayBalancesHandler {
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

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string account;
        std::set<xrpl::AccountID> hotWallets;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
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
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        auto const getHotWalletValidator = [](RippledError errCode) {
            return validation::CustomValidator{
                [errCode](boost::json::value const& value, std::string_view key) -> MaybeError {
                    if (!value.is_string() && !value.is_array())
                        return Error{Status{errCode, std::string(key) + "NotStringOrArray"}};

                    // wallet needs to be an valid accountID or public key
                    auto const wallets =
                        value.is_array() ? value.as_array() : boost::json::array{value};
                    auto const getAccountID = [](auto const& j) -> std::optional<xrpl::AccountID> {
                        if (j.is_string()) {
                            auto const pk = util::parseBase58Wrapper<xrpl::PublicKey>(
                                xrpl::TokenType::AccountPublic,
                                boost::json::value_to<std::string>(j)
                            );

                            if (pk)
                                return xrpl::calcAccountID(*pk);

                            return util::parseBase58Wrapper<xrpl::AccountID>(
                                boost::json::value_to<std::string>(j)
                            );
                        }

                        return {};
                    };

                    for (auto const& wallet : wallets) {
                        if (!getAccountID(wallet))
                            return Error{Status{errCode, std::string(key) + "Malformed"}};
                    }

                    return MaybeError{};
                }
            };
        };

        static auto const kSpecCommon = RpcSpec{
            {JS(account), validation::Required{}, validation::CustomValidators::accountValidator},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator}
        };

        static auto const kSpecV1 = RpcSpec{
            kSpecCommon, {{JS(hotwallet), getHotWalletValidator(xrpl::RpcInvalidHotwallet)}}
        };
        static auto const kSpecV2 =
            RpcSpec{kSpecCommon, {{JS(hotwallet), getHotWalletValidator(xrpl::RpcInvalidParams)}}};

        return apiVersion == 1 ? kSpecV1 : kSpecV2;
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

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
