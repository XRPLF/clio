#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/account_mptoken_issuances/Types.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The account_mptoken_issuances method returns information about all MPTokenIssuance objects
 * the account has created.
 */
class AccountMPTokenIssuancesHandler
    : public rpc::spec::HandlerFor<rpc::spec::handlers::account_mptoken_issuances::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::account_mptoken_issuances::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::account_mptoken_issuances::kLimitMax;
    static constexpr auto kLimitDefault =
        rpc::spec::handlers::account_mptoken_issuances::kLimitDefault;

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

        std::optional<bool> mptImmutableCanLock;
        std::optional<bool> mptImmutableRequireAuth;
        std::optional<bool> mptImmutableCanEscrow;
        std::optional<bool> mptImmutableCanTrade;
        std::optional<bool> mptImmutableCanTransfer;
        std::optional<bool> mptImmutableCanClawback;
        std::optional<bool> mptImmutableCanHoldConfidentialBalance;
        std::optional<bool> mptImmutableMetadata;
        std::optional<bool> mptImmutableTransferFee;

        std::optional<bool> mptCanHoldConfidentialBalance;
        std::optional<std::uint64_t> confidentialOutstandingAmount;
        std::optional<std::string> issuerEncryptionKey;
        std::optional<std::string> auditorEncryptionKey;
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
