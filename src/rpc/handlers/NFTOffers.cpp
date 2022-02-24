#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/RippleState.h>
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
appendNftOfferJson(
    std::shared_ptr<ripple::SLE const> const& offer,
    boost::json::array& offers)
{
    offers.push_back(boost::json::object_kind);
    boost::json::object& obj(offers.back().as_object());

    obj.at("index") = ripple::to_string(offer->key());
    obj.at("flags") = (*offer)[ripple::sfFlags];
    obj.at("owner") = ripple::toBase58(offer->getAccountID(ripple::sfOwner));

    if (offer->isFieldPresent(ripple::sfDestination))
        obj["destination"] =
            ripple::toBase58(offer->getAccountID(ripple::sfDestination));

    if (offer->isFieldPresent(ripple::sfExpiration))
        obj.at("expiration") = offer->getFieldU32(ripple::sfExpiration);

    obj.at("amount") = toBoostJson(offer->getFieldAmount(ripple::sfAmount)
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
    if(!context.backend->fetchLedgerObject(
        directory.key, lgrInfo.seq, context.yield))
        return Status{Error::rpcOBJECT_NOT_FOUND, "notFound"};

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    boost::json::object response = {};
    response["tokenid"] = ripple::to_string(tokenid);
    response["offers"] = boost::json::value(boost::json::array_kind);

    auto& jsonOffers = response["offers"].as_array();

    std::vector<std::shared_ptr<ripple::SLE const>> offers;
    std::uint64_t reserve(limit);
    ripple::uint256 cursor;
    std::uint64_t startHint = 0;

    if (request.contains("marker"))
    {
        // We have a start point. Use limit - 1 from the result and use the
        // very last one for the resume.
        auto const& marker(request.at("marker"));

        if (!marker.is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        if (!cursor.parseHex(marker.as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};

        auto const sle =
            read(ripple::keylet::nftoffer(cursor), lgrInfo, context);

        if (!sle || tokenid != sle->getFieldH256(ripple::sfTokenID))
            return Status{Error::rpcOBJECT_NOT_FOUND, "notFound"};

        if (tokenid != sle->getFieldH256(ripple::sfTokenID))
            return Status{Error::rpcINVALID_PARAMS, "invalidTokenid"};

        startHint = sle->getFieldU64(ripple::sfOfferNode);
        appendNftOfferJson(sle, jsonOffers);
        offers.reserve(reserve);
    }
    else
    {
        // We have no start point, limit should be one higher than requested.
        offers.reserve(++reserve);
    }

    if (!forEachItemAfter(
            lgrInfo,
            directory,
            cursor,
            startHint,
            reserve,
            context,
            [&offers](std::shared_ptr<ripple::SLE const> const& offer) {
                if (offer->getType() == ripple::ltNFTOKEN_OFFER)
                {
                    offers.emplace_back(offer);
                    return true;
                }

                return false;
            }))
    {
        return Status{Error::rpcINVALID_PARAMS, "invalidParams"};
    }

    if (offers.size() == reserve)
    {
        response.at("limit") = limit;
        response.at("marker") = to_string(offers.back()->key());
        offers.pop_back();
    }

    for (auto const& offer : offers)
        appendNftOfferJson(offer, jsonOffers);

    return response;
}

std::variant<ripple::uint256, Status>
getTokenid(boost::json::object const& request)
{
    if (!request.contains("tokenid"))
        return Status{Error::rpcINVALID_PARAMS, "missingTokenid"};

    if (!request.at("tokenid").is_string())
        return Status{Error::rpcINVALID_PARAMS, "tokenidNotString"};

    ripple::uint256 tokenid;
    if (!tokenid.parseHex(request.at("tokenid").as_string().c_str()))
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

} // namespace RPC
