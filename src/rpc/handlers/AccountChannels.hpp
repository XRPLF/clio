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
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_channels::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_channels::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_channels::kLimitDefault;

    /**
     * @brief A struct to hold data for one channel response
     *
     * @note type aligned with SField.h
     */
    struct ChannelResponse {
        std::string channelID;                 ///< Unique ID of the payment channel (hex)
        std::string account;                   ///< Source account that owns the channel
        std::string accountDestination;        ///< Destination account of the channel
        std::string amount;                    ///< Total XRP, in drops, allocated to the channel
        std::string balance;                   ///< XRP, in drops, already paid out of the channel
        std::optional<std::string> publicKey;  ///< Channel public key, base58-encoded
        std::optional<std::string> publicKeyHex;  ///< Channel public key, hex-encoded
        uint32_t settleDelay{};  ///< Seconds the source must wait before closing the channel
        std::optional<uint32_t> expiration;      ///< Time after which the channel can be closed
        std::optional<uint32_t> cancelAfter;     ///< Immutable expiration time of the channel
        std::optional<uint32_t> sourceTag;       ///< Source tag of the channel, if any
        std::optional<uint32_t> destinationTag;  ///< Destination tag of the channel, if any
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::vector<ChannelResponse> channels;  ///< Payment channels owned by the account
        std::string account;                    ///< The account whose channels are returned
        std::string ledgerHash;                 ///< Hash of the ledger used for the request
        uint32_t ledgerIndex{};                 ///< Sequence of the ledger used for the request
        // validated should be sent via framework
        bool validated = true;              ///< Whether the ledger is validated
        uint32_t limit{};                   ///< Maximum number of channels requested
        std::optional<std::string> marker;  ///< Pagination marker to resume from, if more results
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
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
