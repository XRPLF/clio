#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief The account_channels method returns information about an account's Payment Channels. This
 * includes only channels where the specified account is the channel's source, not the destination.
 * All information retrieved is relative to a particular version of the ledger.
 *
 * For more details see: https://xrpl.org/account_channels.html
 */
class AccountChannelsHandler {
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = 10;
    static constexpr auto kLimitMax = 400;
    static constexpr auto kLimitDefault = 200;

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

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string account;
        std::optional<std::string> destinationAccount;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = kLimitDefault;
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
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const kRpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::CustomValidators::accountValidator},
            {JS(destination_account),
             validation::Type<std::string>{},
             validation::CustomValidators::accountValidator},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{kLimitMin, kLimitMax}},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
            {JS(marker), validation::CustomValidators::accountMarkerValidator},
        };

        return kRpcSpec;
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
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);

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
