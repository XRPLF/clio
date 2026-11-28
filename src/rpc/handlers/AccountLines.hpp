#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>
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
 * @brief The account_lines method returns information about an account's trust lines, which contain
 * balances in all non-XRP currencies and assets.
 *
 * For more details see: https://xrpl.org/account_lines.html
 */
class AccountLinesHandler : public rpc::HandlerFor<rpc::spec::handlers::account_lines::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_lines::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_lines::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_lines::kLimitDefault;

    /**
     * @brief A struct to hold data for one line response
     */
    struct LineResponse {
        std::string account;           ///< The counterparty account of the trust line
        std::string balance;           ///< Balance from the perspective of the requested account
        std::string currency;          ///< Currency code of the trust line
        std::string limit;             ///< Limit the requested account set on the line
        std::string limitPeer;         ///< Limit the counterparty set on the line
        uint32_t qualityIn{};          ///< Rate at which incoming balances are valued
        uint32_t qualityOut{};         ///< Rate at which outgoing balances are valued
        std::optional<bool> noRipple;  ///< Whether this account set No Ripple on the line
        std::optional<bool> noRipplePeer;    ///< Whether the peer set No Ripple on the line
        std::optional<bool> authorized;      ///< Whether this account authorized the line
        std::optional<bool> peerAuthorized;  ///< Whether the peer authorized the line
        std::optional<bool> freeze;          ///< Whether this account froze the line
        std::optional<bool> freezePeer;      ///< Whether the peer froze the line
        std::optional<bool> deepFreeze;      ///< Whether this account deep-froze the line
        std::optional<bool> deepFreezePeer;  ///< Whether the peer deep-froze the line
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;              ///< The account whose trust lines are returned
        std::vector<LineResponse> lines;  ///< Trust lines of the account
        std::string ledgerHash;           ///< Hash of the ledger used for the request
        uint32_t ledgerIndex{};           ///< Sequence of the ledger used for the request
        bool validated = true;  ///< Whether the ledger is validated; should be sent via framework
        std::optional<std::string> marker;  ///< Pagination marker to resume from, if more results
        uint32_t limit{};                   ///< Maximum number of trust lines requested
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountLinesHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountLinesHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the AccountLines command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    static void
    addLine(
        std::vector<LineResponse>& lines,
        xrpl::SLE const& lineSle,
        xrpl::AccountID const& account,
        std::optional<xrpl::AccountID> const& peerAccount
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
     * @brief Convert the LineResponse to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param line The line response to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LineResponse const& line);
};

}  // namespace rpc
