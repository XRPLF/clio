#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

namespace RPC {

static void
appendNftOfferJson(ripple::SLE const& offer, boost::json::array& offers)
{
    offers.push_back(boost::json::object_kind);
    boost::json::object& obj(offers.back().as_object());

    obj.at(JS(index)) = ripple::to_string(offer.key());
    obj.at(JS(flags)) = (offer)[ripple::sfFlags];
    obj.at(JS(owner)) = ripple::toBase58(offer.getAccountID(ripple::sfOwner));

    if (offer.isFieldPresent(ripple::sfDestination))
        obj[JS(destination)] =
            ripple::toBase58(offer.getAccountID(ripple::sfDestination));

    if (offer.isFieldPresent(ripple::sfExpiration))
        obj.at(JS(expiration)) = offer.getFieldU32(ripple::sfExpiration);

    obj.at(JS(amount)) = toBoostJson(offer.getFieldAmount(ripple::sfAmount)
                                         .getJson(ripple::JsonOptions::none));
}

static Result
enumerateNFTOffers(
    Context const& context,
    ripple::uint256 const& tokenid,
    ripple::Keylet const& directory)
{
    auto const& request = context.params;

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    // TODO: just check for existence without pulling
    if (!context.app.backend().fetchLedgerObject(
            directory.key, lgrInfo.seq, context.yield))
        return Status{Error::rpcOBJECT_NOT_FOUND, "notFound"};

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    boost::json::object response = {};
    response[JS(nft_id)] = ripple::to_string(tokenid);
    response[JS(offers)] = boost::json::value(boost::json::array_kind);

    auto& jsonOffers = response[JS(offers)].as_array();

    std::vector<ripple::SLE> offers;
    std::uint64_t reserve(limit);
    ripple::uint256 cursor;

    if (request.contains(JS(marker)))
    {
        // We have a start point. Use limit - 1 from the result and use the
        // very last one for the resume.
        auto const& marker(request.at(JS(marker)));

        if (!marker.is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        if (!cursor.parseHex(marker.as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};

        auto const sle =
            read(ripple::keylet::nftoffer(cursor), lgrInfo, context);

        if (!sle || tokenid != sle->getFieldH256(ripple::sfNFTokenID))
            return Status{Error::rpcOBJECT_NOT_FOUND, "notFound"};

        if (tokenid != sle->getFieldH256(ripple::sfNFTokenID))
            return Status{Error::rpcINVALID_PARAMS, "invalidTokenid"};

        appendNftOfferJson(*sle, jsonOffers);
        offers.reserve(reserve);
    }
    else
    {
        // We have no start point, limit should be one higher than requested.
        offers.reserve(++reserve);
    }

    auto result = traverseOwnedNodes(
        context.app.backend(),
        directory,
        cursor,
        0,
        lgrInfo.seq,
        limit,
        {},
        context.yield,
        [&offers](ripple::SLE const& offer) {
            if (offer.getType() == ripple::ltNFTOKEN_OFFER)
            {
                offers.emplace_back(offer);
                return true;
            }

            return false;
        });

    if (auto status = std::get_if<RPC::Status>(&result))
        return *status;

    if (offers.size() == reserve)
    {
        response.at(JS(limit)) = limit;
        response.at(JS(marker)) = to_string(offers.back().key());
        offers.pop_back();
    }

    for (auto const& offer : offers)
        appendNftOfferJson(offer, jsonOffers);

    return response;
}

std::variant<ripple::uint256, Status>
getTokenid(boost::json::object const& request)
{
    if (!request.contains(JS(nft_id)))
        return Status{Error::rpcINVALID_PARAMS, "missingTokenid"};

    if (!request.at(JS(nft_id)).is_string())
        return Status{Error::rpcINVALID_PARAMS, "tokenidNotString"};

    ripple::uint256 tokenid;
    if (!tokenid.parseHex(request.at(JS(nft_id)).as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};

    return tokenid;
}

Result
doNFTOffers(Context const& context, bool sells)
{
    auto const v = getTokenid(context.params);
    if (auto const status = std::get_if<Status>(&v))
        return *status;

    auto const getKeylet = [sells, &v]() {
        if (sells)
            return ripple::keylet::nft_sells(std::get<ripple::uint256>(v));

        return ripple::keylet::nft_buys(std::get<ripple::uint256>(v));
    };

    return enumerateNFTOffers(
        context, std::get<ripple::uint256>(v), getKeylet());
}

Result
doNFTSellOffers(Context const& context)
{
    return doNFTOffers(context, true);
}

Result
doNFTBuyOffers(Context const& context)
{
    return doNFTOffers(context, false);
}

}  // namespace RPC
