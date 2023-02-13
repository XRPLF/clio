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
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <boost/asio/spawn.hpp>

#include <vector>

namespace RPCng {
class AccountChannelsHandler
{
public:
    struct ChannelResponse
    {
        std::string channelID;
        std::string account;
        std::string accountDestination;
        std::string amount;
        std::string balance;
        std::string publicKey;
        std::string publicKeyHex;
        uint64_t settleDelay;
        uint64_t expiration;
        uint64_t cancelAfter;
        uint32_t sourceTag;
        uint32_t destinationTag;
    };

    struct HandlerOutput
    {
        std::vector<ChannelResponse> channels;
        std::string account;
        std::string ledgerHash;
        uint64_t ledgerIndex;
        bool validated;
        uint32_t limit;
        std::string marker;
    };

    struct HandlerInput
    {
        std::string account;
        std::optional<std::string> destinationAccount;
        std::optional<std::string> ledgerHash;
        std::optional<uint64_t> ledgerIndex;
        std::optional<uint32_t> limit;
        std::optional<std::string> marker;
    };

    // clang-format off
    validation::CustomValidator hexFormatCheck = validation::CustomValidator{
        [](boost::json::value const& value, std::string_view key) -> MaybeError {
            ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(value.as_string().c_str()))
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS, "ledgerHashMalformed"}};
            return MaybeError{};
        }
    };
    // clang-format on

    using Input = HandlerInput;
    using Output = HandlerOutput;
    using Result = RPCng::HandlerReturnType<Output>;

    AccountChannelsHandler(
        boost::asio::yield_context& yieldCtx,
        std::shared_ptr<BackendInterface const> const& sharedPtrBackend)
        : yieldCtx_(yieldCtx), sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        // clang-format off
        static const RpcSpec rpcSpec = {
            {"account", validation::Required{}, validation::Type<std::string>{}},
            {"destination_account", validation::Type<std::string>{}},
            {"ledger_hash", validation::Type<std::string>{},hexFormatCheck},
            {"limit", validation::Type<uint32_t>{}},
            {"ledger_index", validation::Type<uint64_t, std::string>{}},
            {"marker", validation::Type<std::string>{}}
        };
        // clang-format on

        return rpcSpec;
    }

    Result
    process(Input input) const;

private:
    // dependencies
    const boost::asio::yield_context& yieldCtx_;
    std::shared_ptr<BackendInterface const> const& sharedPtrBackend_;
};

inline AccountChannelsHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountChannelsHandler::Input>,
    boost::json::value const& jv)
{
    auto jsonObject = jv.as_object();
    std::optional<uint32_t> optLimit;
    if (jsonObject.contains("limit"))
    {
        optLimit = jv.at("limit").as_int64();
    }
    std::optional<std::string> optMarker;
    if (jsonObject.contains("marker"))
    {
        optMarker = jv.at("marker").as_string().c_str();
    }
    std::optional<std::string> optLedgerHash;
    if (jsonObject.contains("ledger_hash"))
    {
        optLedgerHash = jv.at("ledger_hash").as_string().c_str();
    }
    std::optional<std::string> optDestinationAccount;
    if (jsonObject.contains("destination_account"))
    {
        optDestinationAccount =
            jv.at("destination_account").as_string().c_str();
    }
    std::optional<uint64_t> optLedgerIndex;
    if (jsonObject.contains("ledger_index"))
    {
        optLedgerIndex = jv.at("ledger_index").as_uint64();
    }

    return AccountChannelsHandler::Input{
        .ledgerIndex = optLedgerIndex,
        .ledgerHash = optLedgerHash,
        .destinationAccount = optDestinationAccount,
        .marker = optMarker,
        .limit = optLimit,
        .account = jv.at("account").as_string().c_str()};
}

inline void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::Output output)
{
    // jv = {{"computed", output.computed}};
}

}  // namespace RPCng
