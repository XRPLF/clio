#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
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
    : public rpc::HandlerFor<rpc::spec::handlers::account_mptoken_issuances::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_mptoken_issuances::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_mptoken_issuances::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault =
        rpc::spec::handlers::account_mptoken_issuances::kLimitDefault;

    /**
     * @brief A struct to hold data for one MPTokenIssuance response.
     */
    struct MPTokenIssuanceResponse {
        std::string mpTokenIssuanceId;  ///< ID of the MPTokenIssuance
        std::string issuer;             ///< Account that issued the MPT
        uint32_t sequence{};            ///< Sequence number used to create the issuance

        std::optional<uint16_t> transferFee;  ///< Fee charged on transfers, if set
        std::optional<uint8_t> assetScale;    ///< Decimal scale of the MPT, if set

        std::optional<std::uint64_t> maximumAmount;      ///< Maximum amount that can be issued
        std::optional<std::uint64_t> outstandingAmount;  ///< Amount currently held by holders
        std::optional<std::uint64_t> lockedAmount;   ///< Amount currently locked (e.g. escrowed)
        std::optional<std::string> mptokenMetadata;  ///< Arbitrary metadata, hex-encoded
        std::optional<std::string> domainID;         ///< Permissioned domain ID, hex-encoded

        std::optional<bool> mptLocked;       ///< Set if the issuance is locked
        std::optional<bool> mptCanLock;      ///< Set if the issuer can lock the MPT
        std::optional<bool> mptRequireAuth;  ///< Set if holders need issuer authorization
        std::optional<bool> mptCanEscrow;    ///< Set if the MPT can be escrowed
        std::optional<bool> mptCanTrade;     ///< Set if the MPT can be traded on the DEX
        std::optional<bool> mptCanTransfer;  ///< Set if the MPT can be transferred between holders
        std::optional<bool> mptCanClawback;  ///< Set if the issuer can claw back the MPT

        std::optional<bool> mptImmutableCanLock;      ///< Set if the can-lock flag is immutable
        std::optional<bool> mptImmutableRequireAuth;  ///< Set if the require-auth flag is immutable
        std::optional<bool> mptImmutableCanEscrow;    ///< Set if the can-escrow flag is immutable
        std::optional<bool> mptImmutableCanTrade;     ///< Set if the can-trade flag is immutable
        std::optional<bool> mptImmutableCanTransfer;  ///< Set if the can-transfer flag is immutable
        std::optional<bool> mptImmutableCanClawback;  ///< Set if the can-clawback flag is immutable
        /**
         * @brief Set if the can-hold-confidential-balance flag is immutable
         */
        std::optional<bool> mptImmutableCanHoldConfidentialBalance;
        std::optional<bool> mptImmutableMetadata;     ///< Set if the metadata is immutable
        std::optional<bool> mptImmutableTransferFee;  ///< Set if the transfer fee is immutable

        /**
         * @brief Set if confidential balances are allowed
         */
        std::optional<bool> mptCanHoldConfidentialBalance;
        /**
         * @brief Confidential outstanding amount
         */
        std::optional<std::uint64_t> confidentialOutstandingAmount;
        std::optional<std::string> issuerEncryptionKey;   ///< Issuer encryption key, hex-encoded
        std::optional<std::string> auditorEncryptionKey;  ///< Auditor encryption key, hex-encoded
    };

    /**
     * @brief A struct to hold the output data of the command.
     */
    struct Output {
        std::string account;                             ///< The issuer account that was queried
        std::vector<MPTokenIssuanceResponse> issuances;  ///< Issuances created by the account
        std::string ledgerHash;             ///< Hash of the ledger used for the request
        uint32_t ledgerIndex{};             ///< Sequence of the ledger used for the request
        bool validated = true;              ///< Whether the ledger is validated
        std::optional<std::string> marker;  ///< Pagination marker to resume from, if more results
        uint32_t limit{};                   ///< Maximum number of issuances requested
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
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
