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
AccountChannelsHandler::addChannel(
    std::vector<ChannelResponse>& jsonChannels,
    xrpl::SLE const& channelSle
)
{
    ChannelResponse channel;
    channel.channelID = xrpl::to_string(channelSle.key());
    channel.account = xrpl::to_string(channelSle.getAccountID(xrpl::sfAccount));
    channel.accountDestination = xrpl::to_string(channelSle.getAccountID(xrpl::sfDestination));
    channel.amount = channelSle[xrpl::sfAmount].getText();
    channel.balance = channelSle[xrpl::sfBalance].getText();
    channel.settleDelay = channelSle[xrpl::sfSettleDelay];

    if (publicKeyType(channelSle[xrpl::sfPublicKey])) {
        xrpl::PublicKey const pk(channelSle[xrpl::sfPublicKey]);
        channel.publicKey = toBase58(xrpl::TokenType::AccountPublic, pk);
        channel.publicKeyHex = strHex(pk);
    }

    if (auto const& v = channelSle[~xrpl::sfExpiration])
        channel.expiration = v;

    if (auto const& v = channelSle[~xrpl::sfCancelAfter])
        channel.cancelAfter = v;

    if (auto const& v = channelSle[~xrpl::sfSourceTag])
        channel.sourceTag = v;

    if (auto const& v = channelSle[~xrpl::sfDestinationTag])
        channel.destinationTag = v;

    jsonChannels.push_back(channel);
}

AccountChannelsHandler::Result
AccountChannelsHandler::process(
    AccountChannelsHandler::Input const& input,
    Context const& ctx
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountChannel's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        xrpl::keylet::account(*accountID).key,
        lgrInfo.seq,
        ctx.yield
    );

    if (!accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

    auto const destAccountID = input.destinationAccount
        ? accountFromStringStrict(*input.destinationAccount)
        : std::optional<xrpl::AccountID>{};

    Output response;
    auto const addToResponse = [&](xrpl::SLE const sle) {
        if (sle.getType() == xrpl::ltPAYCHAN && sle.getAccountID(xrpl::sfAccount) == accountID &&
            (!destAccountID || *destAccountID == sle.getAccountID(xrpl::sfDestination))) {
            addChannel(response.channels, sle);
        }

        return true;
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,  // NOLINT(bugprone-unchecked-optional-access)
        lgrInfo.seq,
        input.limit,
        input.marker,
        ctx.yield,
        addToResponse
    );

    if (not expectedNext.has_value())
        return Error{expectedNext.error()};

    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    auto const nextMarker = *expectedNext;
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

    if (jsonObject.contains(JS(destination_account))) {
        input.destinationAccount =
            boost::json::value_to<std::string>(jv.at(JS(destination_account)));
    }

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::Output const& output
)
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
        obj[JS(marker)] = *output.marker;

    jv = std::move(obj);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountChannelsHandler::ChannelResponse const& channel
)
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
