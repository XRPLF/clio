#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

namespace json = boost::json;

namespace ripple {

void
tag_invoke(json::value_from_tag, json::value& jv, SLE const& offer)
{
    auto amount = ::RPC::toBoostJson(
        offer.getFieldAmount(sfAmount).getJson(JsonOptions::none));

    json::object obj = {
        {JS(nft_offer_index), to_string(offer.key())},
        {JS(flags), offer[sfFlags]},
        {JS(owner), toBase58(offer.getAccountID(sfOwner))},
        {JS(amount), std::move(amount)},
    };

    if (offer.isFieldPresent(sfDestination))
        obj.insert_or_assign(
            JS(destination), toBase58(offer.getAccountID(sfDestination)));

    if (offer.isFieldPresent(sfExpiration))
        obj.insert_or_assign(JS(expiration), offer.getFieldU32(sfExpiration));

    jv = std::move(obj);
}

}  // namespace ripple

namespace RPC {

Result
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
    if (!context.backend->fetchLedgerObject(
            directory.key, lgrInfo.seq, context.yield))
        return Status{Error::rpcOBJECT_NOT_FOUND, "notFound"};

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    boost::json::object response = {};
    boost::json::array jsonOffers = {};
    response[JS(nft_id)] = ripple::to_string(tokenid);

    std::vector<ripple::SLE> offers;
    auto reserve = limit;
    ripple::uint256 cursor;
    uint64_t startHint = 0;

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

        if (!sle ||
            sle->getFieldU16(ripple::sfLedgerEntryType) !=
                ripple::ltNFTOKEN_OFFER ||
            tokenid != sle->getFieldH256(ripple::sfNFTokenID))
            return Status{Error::rpcINVALID_PARAMS};

        startHint = sle->getFieldU64(ripple::sfNFTokenOfferNode);
        jsonOffers.push_back(json::value_from(*sle));
        offers.reserve(reserve);
    }
    else
    {
        // We have no start point, limit should be one higher than requested.
        offers.reserve(++reserve);
    }

    auto result = traverseOwnedNodes(
        *context.backend,
        directory,
        cursor,
        startHint,
        lgrInfo.seq,
        reserve,
        {},
        context.yield,
        [&offers](ripple::SLE&& offer) {
            if (offer.getType() == ripple::ltNFTOKEN_OFFER)
            {
                offers.push_back(std::move(offer));
                return true;
            }

            return false;
        });

    if (auto status = std::get_if<RPC::Status>(&result))
        return *status;

    if (offers.size() == reserve)
    {
        response[JS(limit)] = limit;
        response[JS(marker)] = to_string(offers.back().key());
        offers.pop_back();
    }

    std::transform(
        std::cbegin(offers),
        std::cend(offers),
        std::back_inserter(jsonOffers),
        [](auto const& offer) {
            // uses tag_invoke at the top of this file
            return json::value_from(offer);
        });

    response.insert_or_assign(JS(offers), std::move(jsonOffers));
    return response;
}

Result
doNFTOffers(Context const& context, bool sells)
{
    auto const v = getNFTID(context.params);
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
