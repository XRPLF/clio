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

#include <rpc/RPCHelpers.h>
#include <rpc/handlers/AccountChannels.h>

namespace RPC {

void
AccountChannelsHandler::addChannel(std::vector<ChannelResponse>& jsonChannels, ripple::SLE const& channelSle) const
{
    ChannelResponse channel;
    channel.channelID = ripple::to_string(channelSle.key());
    channel.account = ripple::to_string(channelSle.getAccountID(ripple::sfAccount));
    channel.accountDestination = ripple::to_string(channelSle.getAccountID(ripple::sfDestination));
    channel.amount = channelSle[ripple::sfAmount].getText();
    channel.balance = channelSle[ripple::sfBalance].getText();
    channel.settleDelay = channelSle[ripple::sfSettleDelay];

    if (publicKeyType(channelSle[ripple::sfPublicKey]))
    {
        ripple::PublicKey const pk(channelSle[ripple::sfPublicKey]);
        channel.publicKey = toBase58(ripple::TokenType::AccountPublic, pk);
        channel.publicKeyHex = strHex(pk);
    }

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
AccountChannelsHandler::process(AccountChannelsHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto const destAccountID = input.destinationAccount ? accountFromStringStrict(input.destinationAccount.value())
                                                        : std::optional<ripple::AccountID>{};

    Output response;
    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltPAYCHAN && sle.getAccountID(ripple::sfAccount) == accountID &&
            (!destAccountID || *destAccountID == sle.getAccountID(ripple::sfDestination)))
        {
            addChannel(response.channels, sle);
        }

        return true;
    };

    auto const next = ngTraverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse);

    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    auto const nextMarker = std::get<AccountCursor>(next);
    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountChannelsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountChannelsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountChannelsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = jv.at(JS(account)).as_string().c_str();

    if (jsonObject.contains(JS(limit)))
        input.limit = jv.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jv.at(JS(marker)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(destination_account)))
        input.destinationAccount = jv.at(JS(destination_account)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountChannelsHandler::Output const& output)
{
    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(channels), output.channels},
    };

    if (output.marker)
        obj[JS(marker)] = output.marker.value();

    jv = std::move(obj);
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountChannelsHandler::ChannelResponse const& channel)
{
    auto obj = boost::json::object{
        {JS(channel_id), channel.channelID},
        {JS(account), channel.account},
        {JS(destination_account), channel.accountDestination},
        {JS(amount), channel.amount},
        {JS(balance), channel.balance},
        {JS(settle_delay), channel.settleDelay},
    };

    if (channel.publicKey)
        obj[JS(public_key)] = *(channel.publicKey);

    if (channel.publicKeyHex)
        obj[JS(public_key_hex)] = *(channel.publicKeyHex);

    if (channel.expiration)
        obj[JS(expiration)] = *(channel.expiration);

    if (channel.cancelAfter)
        obj[JS(cancel_after)] = *(channel.cancelAfter);

    if (channel.sourceTag)
        obj[JS(source_tag)] = *(channel.sourceTag);

    if (channel.destinationTag)
        obj[JS(destination_tag)] = *(channel.destinationTag);

    jv = std::move(obj);
}
}  // namespace RPC
