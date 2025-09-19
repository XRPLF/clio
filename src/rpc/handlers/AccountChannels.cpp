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

#include "rpc/handlers/AccountChannels.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/tokens.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

void
AccountChannelsHandler::addChannel(std::vector<ChannelResponse>& jsonChannels, ripple::SLE const& channelSle)
{
    ChannelResponse channel;
    channel.channelID = ripple::to_string(channelSle.key());
    channel.account = ripple::to_string(channelSle.getAccountID(ripple::sfAccount));
    channel.accountDestination = ripple::to_string(channelSle.getAccountID(ripple::sfDestination));
    channel.amount = channelSle[ripple::sfAmount].getText();
    channel.balance = channelSle[ripple::sfBalance].getText();
    channel.settleDelay = channelSle[ripple::sfSettleDelay];

    if (publicKeyType(channelSle[ripple::sfPublicKey])) {
        ripple::PublicKey const pk(channelSle[ripple::sfPublicKey]);
        channel.publicKey = toBase58(ripple::TokenType::AccountPublic, pk);
        channel.publicKeyHex = strHex(pk);
    }

    if (auto const& v = channelSle[~ripple::sfExpiration])
        channel.expiration = v;

    if (auto const& v = channelSle[~ripple::sfCancelAfter])
        channel.cancelAfter = v;

    if (auto const& v = channelSle[~ripple::sfSourceTag])
        channel.sourceTag = v;

    if (auto const& v = channelSle[~ripple::sfDestinationTag])
        channel.destinationTag = v;

    jsonChannels.push_back(channel);
}

AccountChannelsHandler::Result
AccountChannelsHandler::process(AccountChannelsHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountChannel's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (!expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = expectedLgrInfo.value();
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto const destAccountID = input.destinationAccount ? accountFromStringStrict(input.destinationAccount.value())
                                                        : std::optional<ripple::AccountID>{};

    Output response;
    auto const addToResponse = [&](ripple::SLE const sle) {
        if (sle.getType() == ripple::ltPAYCHAN && sle.getAccountID(ripple::sfAccount) == accountID &&
            (!destAccountID || *destAccountID == sle.getAccountID(ripple::sfDestination))) {
            addChannel(response.channels, sle);
        }

        return true;
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse
    );

    if (!expectedNext.has_value())
        return Error{expectedNext.error()};

    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    auto const nextMarker = expectedNext.value();
    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountChannelsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountChannelsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountChannelsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(destination_account)))
        input.destinationAccount = boost::json::value_to<std::string>(jv.at(JS(destination_account)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = util::integralValueAs<uint32_t>(jv.at(JS(ledger_index)));
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountChannelsHandler::Output const& output)
{
    using boost::json::value_from;

    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(channels), value_from(output.channels)},
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
}  // namespace rpc
