#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief The account_mptokens method returns information about the MPTokens the account currently
 * holds.
 */
class AccountMPTokensHandler {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = 10;
    static constexpr auto kLimitMax = 400;
    static constexpr auto kLimitDefault = 200;

    /**
     * @brief A struct to hold data for one MPToken response.
     */
    struct MPTokenResponse {
        std::string mpTokenId;
        std::string account;
        std::string mpTokenIssuanceId;
        uint64_t mptAmount{};
        std::optional<uint64_t> lockedAmount;

        std::optional<bool> mptLocked;
        std::optional<bool> mptAuthorized;
    };

    /**
     * @brief A struct to hold the output data of the command.
     */
    struct Output {
        std::string account;
        std::vector<MPTokenResponse> mpts;
        std::string ledgerHash;
        uint32_t ledgerIndex{};
        bool validated = true;
        std::optional<std::string> marker;
        uint32_t limit{};
    };

    /**
     * @brief A struct to hold the input data for the command.
     */
    struct Input {
        std::string account;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = kLimitDefault;
        std::optional<std::string> marker;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountMPTokensHandler object.
     *
     * @param sharedPtrBackend The backend to use.
     */
    AccountMPTokensHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Returns the API specification for the command.
     *
     * @param apiVersion The API version to return the spec for.
     * @return The spec for the given API version.
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const kRpcSpec = RpcSpec{
            {JS(account),
             validation::Required{},
             meta::WithCustomError{
                 validation::CustomValidators::accountValidator,
                 Status(RippledError::RpcActMalformed)
             }},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{kLimitMin, kLimitMax}},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
            {JS(marker), validation::CustomValidators::accountMarkerValidator},
            {JS(ledger), check::Deprecated{}},
        };

        return kRpcSpec;
    }

    /**
     * @brief Process the AccountMPTokens command.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @return The result of the operation.
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    static void
    addMPToken(std::vector<MPTokenResponse>& mpts, xrpl::SLE const& sle);

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

    /**
     * @brief Convert the MPTokenResponse to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param mptoken The MPToken response to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MPTokenResponse const& mptoken);
};

}  // namespace rpc
