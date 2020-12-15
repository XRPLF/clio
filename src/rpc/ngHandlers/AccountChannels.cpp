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

#include <rpc/RPC.h>
#include <rpc/RPCHelpers.h>
#include <rpc/ngHandlers/AccountChannels.h>

namespace RPCng {

void
AccountChannelsHandler::addChannel(
    std::vector<ChannelResponse>& jsonChannels,
    ripple::SLE const& channelSle) const
{
    ChannelResponse channel;
    channel.channelID = ripple::to_string(channelSle.key());
    channel.account =
        ripple::to_string(channelSle.getAccountID(ripple::sfAccount));
    channel.accountDestination =
        ripple::to_string(channelSle.getAccountID(ripple::sfDestination));
    channel.amount = channelSle[ripple::sfAmount].getText();
    channel.balance = channelSle[ripple::sfBalance].getText();
    if (publicKeyType(channelSle[ripple::sfPublicKey]))
    {
        ripple::PublicKey const pk(channelSle[ripple::sfPublicKey]);
        channel.publicKey = toBase58(ripple::TokenType::AccountPublic, pk);
        channel.publicKeyHex = strHex(pk);
    }
    channel.settleDelay = channelSle[ripple::sfSettleDelay];
    if (auto const& v = channelSle[~ripple::sfExpiration])
        channel.expiration = *v;
    if (auto const& v = channelSle[~ripple::sfCancelAfter])
        channel.cancelAfter = *v;
    if (auto const& v = channelSle[~ripple::sfSourceTag])
        channel.sourceTag = *v;
    if (auto const& v = channelSle[~ripple::sfDestinationTag])
        channel.destinationTag = *v;

    jsonChannels.push_back(channel);
}

AccountChannelsHandler::Result
AccountChannelsHandler::process(
    AccountChannelsHandler::Input input,
    boost::asio::yield_context& yield) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = RPC::getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_,
        yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence);

    if (auto status = std::get_if<RPC::Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);

    // no need to check the return value, validator check for us
    auto const accountID = RPC::accountFromStringStrict(input.account);

    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        ripple::keylet::account(*accountID).key, lgrInfo.seq, yield);
    if (!accountLedgerObject)
        return Error{RPC::Status{
            RPC::RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto const destAccountID = input.destinationAccount
        ? RPC::accountFromStringStrict(input.destinationAccount.value())
        : std::optional<ripple::AccountID>{};

    Output response;
    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltPAYCHAN &&
            sle.getAccountID(ripple::sfAccount) == accountID &&
            (!destAccountID ||
             *destAccountID == sle.getAccountID(ripple::sfDestination)))
        {
            addChannel(response.channels, sle);
        }
        return true;
    };

    auto const next = RPC::ngTraverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        input.limit,
        input.marker,
        yield,
        addToResponse);

    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    auto const nextMarker = std::get<RPC::AccountCursor>(next);
    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountChannelsHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountChannelsHandler::Input>,
    boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    AccountChannelsHandler::Input input;
    input.account = jv.at("account").as_string().c_str();
    if (jsonObject.contains("limit"))
    {
        input.limit = jv.at("limit").as_int64();
    }
    if (jsonObject.contains("marker"))
    {
        input.marker = jv.at("marker").as_string().c_str();
    }
    if (jsonObject.contains("ledger_hash"))
    {
        input.ledgerHash = jv.at("ledger_hash").as_string().c_str();
    }
    if (jsonObject.contains("destination_account"))
    {
        input.destinationAccount =
            jv.at("destination_account").as_string().c_str();
    }
    if (jsonObject.contains("ledger_index"))
    {
        if (!jsonObject.at("ledger_index").is_string())
        {
            input.ledgerIndex = jv.at("ledger_index").as_int64();
        }
        else if (jsonObject.at("ledger_index").as_string() != "validated")
        {
            input.ledgerIndex =
                std::stoi(jv.at("ledger_index").as_string().c_str());
        }
    }

    return input;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::Output const& output)
{
    boost::json::object obj;
    obj = {
        {"account", output.account},
        {"ledger_hash", output.ledgerHash},
        {"ledger_index", output.ledgerIndex},
        {"validated", output.validated},
        {"limit", output.limit},
        {"channels", output.channels}};
    if (output.marker)
        obj["marker"] = output.marker.value();
    jv = obj;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::ChannelResponse const& channel)
{
    boost::json::object obj;
    obj = {
        {"channel_id", channel.channelID},
        {"account", channel.account},
        {"account_destination", channel.accountDestination},
        {"amount", channel.amount},
        {"balance", channel.balance},
        {"settle_delay", channel.settleDelay}};
    if (channel.publicKey)
        obj["public_key"] = *(channel.publicKey);
    if (channel.publicKeyHex)
        obj["public_key_hex"] = *(channel.publicKeyHex);
    if (channel.expiration)
        obj["expiration"] = *(channel.expiration);
    if (channel.cancelAfter)
        obj["cancel_after"] = *(channel.cancelAfter);
    if (channel.sourceTag)
        obj["source_tag"] = *(channel.sourceTag);
    if (channel.destinationTag)
        obj["destination_tag"] = *(channel.destinationTag);
    jv = obj;
}
}  // namespace RPCng
