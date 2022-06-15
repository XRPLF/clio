#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/TrustLine.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>

namespace RPC {

void
addOffer(boost::json::array& offersJson, ripple::SLE const& offer)
{
    auto quality = getQuality(offer.getFieldH256(ripple::sfBookDirectory));
    ripple::STAmount rate = ripple::amountFromQuality(quality);

    ripple::STAmount takerPays = offer.getFieldAmount(ripple::sfTakerPays);
    ripple::STAmount takerGets = offer.getFieldAmount(ripple::sfTakerGets);

    boost::json::object obj;

    if (!takerPays.native())
    {
        obj[JS(taker_pays)] = boost::json::value(boost::json::object_kind);
        boost::json::object& takerPaysJson = obj.at(JS(taker_pays)).as_object();

        takerPaysJson[JS(value)] = takerPays.getText();
        takerPaysJson[JS(currency)] =
            ripple::to_string(takerPays.getCurrency());
        takerPaysJson[JS(issuer)] = ripple::to_string(takerPays.getIssuer());
    }
    else
    {
        obj[JS(taker_pays)] = takerPays.getText();
    }

    if (!takerGets.native())
    {
        obj[JS(taker_gets)] = boost::json::value(boost::json::object_kind);
        boost::json::object& takerGetsJson = obj.at(JS(taker_gets)).as_object();

        takerGetsJson[JS(value)] = takerGets.getText();
        takerGetsJson[JS(currency)] =
            ripple::to_string(takerGets.getCurrency());
        takerGetsJson[JS(issuer)] = ripple::to_string(takerGets.getIssuer());
    }
    else
    {
        obj[JS(taker_gets)] = takerGets.getText();
    }

    obj[JS(seq)] = offer.getFieldU32(ripple::sfSequence);
    obj[JS(flags)] = offer.getFieldU32(ripple::sfFlags);
    obj[JS(quality)] = rate.getText();
    if (offer.isFieldPresent(ripple::sfExpiration))
        obj[JS(expiration)] = offer.getFieldU32(ripple::sfExpiration);

    offersJson.push_back(obj);
};

Result
doAccountOffers(Context const& context)
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
    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;
    response[JS(offers)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonLines = response.at(JS(offers)).as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltOFFER)
        {
            addOffer(jsonLines, sle);
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

    if (auto status = std::get_if<RPC::Status>(&next))
        return *status;

    auto nextMarker = std::get<RPC::AccountCursor>(next);

    if (nextMarker.isNonZero())
        response[JS(marker)] = nextMarker.toString();

    return response;
}

}  // namespace RPC
