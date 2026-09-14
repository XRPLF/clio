#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
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
    : public rpc::spec::HandlerFor<rpc::spec::handlers::account_mptokens::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::account_mptokens::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::account_mptokens::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_mptokens::kLimitDefault;

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

        std::optional<std::string> confidentialBalanceInbox;
        std::optional<std::string> confidentialBalanceSpending;
        std::optional<uint32_t> confidentialBalanceVersion;
        std::optional<std::string> issuerEncryptedBalance;
        std::optional<std::string> auditorEncryptedBalance;
        std::optional<std::string> holderEncryptionKey;
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
