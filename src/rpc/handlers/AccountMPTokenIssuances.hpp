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
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief The account_mptoken_issuances method returns information about all MPTokenIssuance objects
 * the account has created.
 */
class AccountMPTokenIssuancesHandler {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = 10;
    static constexpr auto kLimitMax = 400;
    static constexpr auto kLimitDefault = 200;

    /**
     * @brief A struct to hold data for one MPTokenIssuance response.
     */
    struct MPTokenIssuanceResponse {
        std::string mpTokenIssuanceId;
        std::string issuer;
        uint32_t sequence{};

        std::optional<uint16_t> transferFee;
        std::optional<uint8_t> assetScale;

        std::optional<std::uint64_t> maximumAmount;
        std::optional<std::uint64_t> outstandingAmount;
        std::optional<std::uint64_t> lockedAmount;
        std::optional<std::string> mptokenMetadata;
        std::optional<std::string> domainID;

        std::optional<bool> mptLocked;
        std::optional<bool> mptCanLock;
        std::optional<bool> mptRequireAuth;
        std::optional<bool> mptCanEscrow;
        std::optional<bool> mptCanTrade;
        std::optional<bool> mptCanTransfer;
        std::optional<bool> mptCanClawback;

        std::optional<bool> mptCanMutateCanLock;
        std::optional<bool> mptCanMutateRequireAuth;
        std::optional<bool> mptCanMutateCanEscrow;
        std::optional<bool> mptCanMutateCanTrade;
        std::optional<bool> mptCanMutateCanTransfer;
        std::optional<bool> mptCanMutateCanClawback;
        std::optional<bool> mptCanMutateMetadata;
        std::optional<bool> mptCanMutateTransferFee;
    };

    /**
     * @brief A struct to hold the output data of the command.
     */
    struct Output {
        std::string account;
        std::vector<MPTokenIssuanceResponse> issuances;
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
     * @brief Construct a new AccountMPTokenIssuancesHandler object.
     *
     * @param sharedPtrBackend The backend to use.
     */
    AccountMPTokenIssuancesHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
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
     * @brief Process the AccountMPTokenIssuances command.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @return The result of the operation.
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    static void
    addMPTokenIssuance(
        std::vector<MPTokenIssuanceResponse>& issuances,
        xrpl::SLE const& sle,
        xrpl::AccountID const& account
    );

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
     * @brief Convert the MPTokenIssuanceResponse to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param issuance The MPTokenIssuance response to convert
     */
    friend void
    tag_invoke(
        boost::json::value_from_tag,
        boost::json::value& jv,
        MPTokenIssuanceResponse const& issuance
    );
};

}  // namespace rpc
