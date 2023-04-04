//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <boost/asio/spawn.hpp>

#include <vector>

namespace RPCng {
class AccountChannelsHandler
{
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    // type align with SField.h
    struct ChannelResponse
    {
        std::string channelID;
        std::string account;
        std::string accountDestination;
        std::string amount;
        std::string balance;
        std::optional<std::string> publicKey;
        std::optional<std::string> publicKeyHex;
        uint32_t settleDelay;
        std::optional<uint32_t> expiration;
        std::optional<uint32_t> cancelAfter;
        std::optional<uint32_t> sourceTag;
        std::optional<uint32_t> destinationTag;
    };

    struct Output
    {
        std::vector<ChannelResponse> channels;
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        // validated should be sent via framework
        bool validated = true;
        uint32_t limit;
        std::optional<std::string> marker;
    };

    struct Input
    {
        std::string account;
        std::optional<std::string> destinationAccount;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = 50;
        std::optional<std::string> marker;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    AccountChannelsHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        // clang-format off
        static auto const  rpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(destination_account), validation::Type<std::string>{},validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(limit), validation::Type<uint32_t>{},validation::Between{10,400}},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(marker), validation::AccountMarkerValidator}
        };
        // clang-format on

        return rpcSpec;
    }

    Result
    process(Input input, Context ctx) const;

private:
    void
    addChannel(std::vector<ChannelResponse>& jsonLines, ripple::SLE const& line)
        const;
};

AccountChannelsHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountChannelsHandler::Input>,
    boost::json::value const& jv);

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::Output const& output);

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::ChannelResponse const& channel);
}  // namespace RPCng
