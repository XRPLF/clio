#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>

#include <rpc/RPCHelpers.h>

namespace RPC {

void
addChannel(boost::json::array& jsonLines, ripple::SLE const& line)
{
    boost::json::object jDst;
    jDst[JS(channel_id)] = ripple::to_string(line.key());
    jDst[JS(account)] = ripple::to_string(line.getAccountID(ripple::sfAccount));
    jDst[JS(destination_account)] =
        ripple::to_string(line.getAccountID(ripple::sfDestination));
    jDst[JS(amount)] = line[ripple::sfAmount].getText();
    jDst[JS(balance)] = line[ripple::sfBalance].getText();
    if (publicKeyType(line[ripple::sfPublicKey]))
    {
        ripple::PublicKey const pk(line[ripple::sfPublicKey]);
        jDst[JS(public_key)] = toBase58(ripple::TokenType::AccountPublic, pk);
        jDst[JS(public_key_hex)] = strHex(pk);
    }
    jDst[JS(settle_delay)] = line[ripple::sfSettleDelay];
    if (auto const& v = line[~ripple::sfExpiration])
        jDst[JS(expiration)] = *v;
    if (auto const& v = line[~ripple::sfCancelAfter])
        jDst[JS(cancel_after)] = *v;
    if (auto const& v = line[~ripple::sfSourceTag])
        jDst[JS(source_tag)] = *v;
    if (auto const& v = line[~ripple::sfDestinationTag])
        jDst[JS(destination_tag)] = *v;

    jsonLines.push_back(jDst);
}

Result
doAccountChannels(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    auto rawAcct = context.backend->fetchLedgerObject(
        ripple::keylet::account(accountID).key, lgrInfo.seq, context.yield);

    if (!rawAcct)
        return Status{Error::rpcACT_NOT_FOUND, "accountNotFound"};

    ripple::AccountID destAccount;
    if (auto const status =
            getAccount(request, destAccount, JS(destination_account));
        status)
        return status;

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    std::optional<std::string> marker = {};
    if (request.contains(JS(marker)))
    {
        if (!request.at(JS(marker)).is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        marker = request.at(JS(marker)).as_string().c_str();
    }

    response[JS(account)] = ripple::to_string(accountID);
    response[JS(channels)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonChannels = response.at(JS(channels)).as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltPAYCHAN &&
            sle.getAccountID(ripple::sfAccount) == accountID &&
            (!destAccount ||
             destAccount == sle.getAccountID(ripple::sfDestination)))
        {
            addChannel(jsonChannels, sle);
        }

        return true;
    };

    auto next = traverseOwnedNodes(
        *context.backend,
        accountID,
        lgrInfo.seq,
        limit,
        marker,
        context.yield,
        addToResponse);

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    if (auto status = std::get_if<RPC::Status>(&next))
        return *status;

    auto nextMarker = std::get<RPC::AccountCursor>(next);

    if (nextMarker.isNonZero())
        response[JS(marker)] = nextMarker.toString();

    return response;
}

}  // namespace RPC
