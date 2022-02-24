#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/TrustLine.h>
#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/nftPageMask.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>

namespace RPC {

std::unordered_map<std::string, ripple::LedgerEntryType> types{
    {"state", ripple::ltRIPPLE_STATE},
    {"ticket", ripple::ltTICKET},
    {"signer_list", ripple::ltSIGNER_LIST},
    {"payment_channel", ripple::ltPAYCHAN},
    {"offer", ripple::ltOFFER},
    {"escrow", ripple::ltESCROW},
    {"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
    {"check", ripple::ltCHECK},
    {"nft_page", ripple::ltNFTOKEN_PAGE},
    {"nft_offer", ripple::ltNFTOKEN_OFFER}};

Result
doAccountNFTs(Context const& context)
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

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    // TODO: just check for existence without pulling
    if (!context.backend->fetchLedgerObject(
            ripple::keylet::account(accountID).key, lgrInfo.seq, context.yield))
        return Status{Error::rpcACT_NOT_FOUND, "accountNotFound"};

    std::uint32_t limit = 200;
    if (auto const status = getLimit(request, limit); status)
        return status;

    ripple::uint256 marker;
    if (auto const status = getHexMarker(request, marker); status)
        return status;

    auto const first =
        ripple::keylet::nftpage(ripple::keylet::nftpage_min(accountID), marker);
    auto const last = ripple::keylet::nftpage_max(accountID);

    auto const key =
        context.backend
            ->fetchSuccessorKey(first.key, lgrInfo.seq, context.yield)
            .value_or(last.key);
    auto const blob = context.backend->fetchLedgerObject(
        ripple::Keylet(ripple::ltNFTOKEN_PAGE, key).key,
        lgrInfo.seq,
        context.yield);

    std::optional<ripple::SLE const> cp{
        ripple::SLE{ripple::SerialIter{blob->data(), blob->size()}, key}};

    std::uint32_t cnt = 0;
    response[JS(account_nfts)] = boost::json::value(boost::json::array_kind);
    auto& nfts = response.at(JS(account_nfts)).as_array();

    bool pastMarker = marker.isZero();
    ripple::uint256 const maskedMarker = marker & ripple::nft::pageMask;

    // Continue iteration from the current page:
    while (true)
    {
        auto arr = cp->getFieldArray(ripple::sfNFTokens);

        for (auto const& o : arr)
        {
            ripple::uint256 const nftokenID = o[ripple::sfNFTokenID];
            ripple::uint256 const maskedNftokenID =
                nftokenID & ripple::nft::pageMask;

            if (!pastMarker && maskedNftokenID < maskedMarker)
                continue;

            if (!pastMarker && maskedNftokenID == maskedMarker &&
                nftokenID <= marker)
                continue;

            {
                nfts.push_back(
                    toBoostJson(o.getJson(ripple::JsonOptions::none)));
                auto& obj = nfts.back().as_object();

                // Pull out the components of the nft ID.
                obj[SFS(sfFlags)] = ripple::nft::getFlags(nftokenID);
                obj[SFS(sfIssuer)] =
                    to_string(ripple::nft::getIssuer(nftokenID));
                obj[SFS(sfNFTokenTaxon)] =
                    ripple::nft::toUInt32(ripple::nft::getTaxon(nftokenID));
                obj[JS(nft_serial)] = ripple::nft::getSerial(nftokenID);

                if (std::uint16_t xferFee = {
                        ripple::nft::getTransferFee(nftokenID)})
                    obj[SFS(sfTransferFee)] = xferFee;
            }

            if (++cnt == limit)
            {
                response[JS(limit)] = limit;
                response[JS(marker)] =
                    to_string(o.getFieldH256(ripple::sfNFTokenID));
                return response;
            }
        }

        if (auto npm = (*cp)[~ripple::sfNextPageMin])
        {
            auto const nextKey = ripple::Keylet(ripple::ltNFTOKEN_PAGE, *npm);
            auto const nextBlob = context.backend->fetchLedgerObject(
                nextKey.key, lgrInfo.seq, context.yield);

            cp.emplace(ripple::SLE{
                ripple::SerialIter{nextBlob->data(), nextBlob->size()},
                nextKey.key});
        }
        else
            break;
    }

    response[JS(account)] = ripple::toBase58(accountID);
    return response;
}

Result
doAccountObjects(Context const& context)
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

    std::uint32_t limit = 200;
    if (auto const status = getLimit(request, limit); status)
        return status;

    std::optional<std::string> marker = {};
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        marker = request.at("marker").as_string().c_str();
    }

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains(JS(type)))
    {
        if (!request.at(JS(type)).is_string())
            return Status{Error::rpcINVALID_PARAMS, "typeNotString"};

        std::string typeAsString = request.at(JS(type)).as_string().c_str();
        if (types.find(typeAsString) == types.end())
            return Status{Error::rpcINVALID_PARAMS, "typeInvalid"};

        objectType = types[typeAsString];
    }

    response[JS(account)] = ripple::to_string(accountID);
    response[JS(account_objects)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects =
        response.at(JS(account_objects)).as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (!objectType || objectType == sle.getType())
        {
            jsonObjects.push_back(toJson(sle));
        }
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
