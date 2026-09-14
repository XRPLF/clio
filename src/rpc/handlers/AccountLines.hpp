#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
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
class AccountLinesHandler
    : public rpc::spec::HandlerFor<rpc::spec::handlers::account_lines::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::account_lines::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::account_lines::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_lines::kLimitDefault;

    /**
     * @brief A struct to hold data for one line response
     */
    struct LineResponse {
        std::string account;
        std::string balance;
        std::string currency;
        std::string limit;
        std::string limitPeer;
        uint32_t qualityIn{};
        uint32_t qualityOut{};
        std::optional<bool> noRipple;
        std::optional<bool> noRipplePeer;
        std::optional<bool> authorized;
        std::optional<bool> peerAuthorized;
        std::optional<bool> freeze;
        std::optional<bool> freezePeer;
        std::optional<bool> deepFreeze;
        std::optional<bool> deepFreezePeer;
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;
        std::vector<LineResponse> lines;
        std::string ledgerHash;
        uint32_t ledgerIndex{};
        bool validated = true;  // should be sent via framework
        std::optional<std::string> marker;
        uint32_t limit{};
    };

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
