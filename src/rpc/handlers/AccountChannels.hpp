#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The account_channels method returns information about an account's Payment Channels. This
 * includes only channels where the specified account is the channel's source, not the destination.
 * All information retrieved is relative to a particular version of the ledger.
 *
 * For more details see: https://xrpl.org/account_channels.html
 */
class AccountChannelsHandler
    : public rpc::HandlerFor<rpc::spec::handlers::account_channels::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::account_channels::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::account_channels::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_channels::kLimitDefault;

    /**
     * @brief A struct to hold data for one channel response
     *
     * @note type aligned with SField.h
     */
    struct ChannelResponse {
        std::string channelID;
        std::string account;
        std::string accountDestination;
        std::string amount;
        std::string balance;
        std::optional<std::string> publicKey;
        std::optional<std::string> publicKeyHex;
        uint32_t settleDelay{};
        std::optional<uint32_t> expiration;
        std::optional<uint32_t> cancelAfter;
        std::optional<uint32_t> sourceTag;
        std::optional<uint32_t> destinationTag;
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::vector<ChannelResponse> channels;
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex{};
        // validated should be sent via framework
        bool validated = true;
        uint32_t limit{};
        std::optional<std::string> marker;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountChannelsHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountChannelsHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the AccountChannels command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    static void
    addChannel(std::vector<ChannelResponse>& jsonChannels, xrpl::SLE const& channelSle);

    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert the ChannelResponse to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param channel The channel response to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ChannelResponse const& channel);
};
}  // namespace rpc
