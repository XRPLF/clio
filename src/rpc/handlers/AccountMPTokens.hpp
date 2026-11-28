#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_mptokens/Types.hpp>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The account_mptokens method returns information about the MPTokens the account currently
 * holds.
 */
class AccountMPTokensHandler
    : public rpc::HandlerFor<rpc::spec::handlers::account_mptokens::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_mptokens::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_mptokens::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_mptokens::kLimitDefault;

    /**
     * @brief A struct to hold data for one MPToken response.
     */
    struct MPTokenResponse {
        std::string mpTokenId;                 ///< Ledger key of the MPToken object (hex)
        std::string account;                   ///< Account holding the MPToken
        std::string mpTokenIssuanceId;         ///< ID of the MPTokenIssuance this token belongs to
        uint64_t mptAmount{};                  ///< Amount of the MPT held
        std::optional<uint64_t> lockedAmount;  ///< Amount currently locked (e.g. escrowed)

        std::optional<bool> mptLocked;      ///< Set if the holder's MPToken is locked
        std::optional<bool> mptAuthorized;  ///< Set if the issuer authorized the holder

        std::optional<std::string> confidentialBalanceInbox;  ///< Encrypted inbox balance, hex
        /**
         * @brief Encrypted spending balance, hex
         */
        std::optional<std::string> confidentialBalanceSpending;
        std::optional<uint32_t> confidentialBalanceVersion;  ///< Confidential balance version
        std::optional<std::string> issuerEncryptedBalance;   ///< Balance encrypted for issuer, hex
        std::optional<std::string> auditorEncryptedBalance;  ///< Balance encrypted for auditor, hex
        std::optional<std::string> holderEncryptionKey;      ///< Holder encryption key, hex
    };

    /**
     * @brief A struct to hold the output data of the command.
     */
    struct Output {
        std::string account;                ///< The holder account that was queried
        std::vector<MPTokenResponse> mpts;  ///< MPTokens held by the account
        std::string ledgerHash;             ///< Hash of the ledger used for the request
        uint32_t ledgerIndex{};             ///< Sequence of the ledger used for the request
        bool validated = true;              ///< Whether the ledger is validated
        std::optional<std::string> marker;  ///< Pagination marker to resume from, if more results
        uint32_t limit{};                   ///< Maximum number of MPTokens requested
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
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
     * @brief Convert the MPTokenResponse to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param mptoken The MPToken response to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MPTokenResponse const& mptoken);
};

}  // namespace rpc
