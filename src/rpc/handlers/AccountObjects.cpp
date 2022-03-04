#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/RippleState.h>
#include <ripple/app/tx/impl/details/NFTokenUtils.h>
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

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    // TODO: just check for existence without pulling
    if (!context.backend->fetchLedgerObject(
            ripple::keylet::account(*accountID).key,
            lgrInfo.seq,
            context.yield))
        return Status{Error::rpcACT_NOT_FOUND, "accountNotFound"};

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    ripple::uint256 cursor;
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        if (!cursor.parseHex(request.at("marker").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    auto const first = ripple::keylet::nftpage(
        ripple::keylet::nftpage_min(*accountID), cursor);
    auto const last = ripple::keylet::nftpage_max(*accountID);

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
    response["account_nfts"] = boost::json::value(boost::json::array_kind);
    auto& nfts = response.at("account_nfts").as_array();

    // Continue iteration from the current page:
    while (cp)
    {
        auto arr = cp->getFieldArray(ripple::sfNonFungibleTokens);

        for (auto const& o : arr)
        {
            if (o.getFieldH256(ripple::sfTokenID) <= cursor)
                continue;

            {
                nfts.push_back(
                    toBoostJson(o.getJson(ripple::JsonOptions::none)));
                auto& obj = nfts.back().as_object();

                // Pull out the components of the nft ID.
                ripple::uint256 const tokenID = o[ripple::sfTokenID];
                obj[ripple::sfFlags.jsonName.c_str()] =
                    ripple::nft::getFlags(tokenID);
                obj[ripple::sfIssuer.jsonName.c_str()] =
                    to_string(ripple::nft::getIssuer(tokenID));
                obj[ripple::sfTokenTaxon.jsonName.c_str()] =
                    ripple::nft::getTaxon(tokenID);
                obj[ripple::jss::nft_serial.c_str()] =
                    ripple::nft::getSerial(tokenID);

                if (std::uint16_t xferFee = {
                        ripple::nft::getTransferFee(tokenID)})
                    obj[ripple::sfTransferFee.jsonName.c_str()] = xferFee;
            }

            if (++cnt == limit)
            {
                response[ripple::jss::limit.c_str()] = limit;
                response[ripple::jss::marker.c_str()] =
                    to_string(o.getFieldH256(ripple::sfTokenID));
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
            cp = std::nullopt;
    }

    response[ripple::jss::account.c_str()] = ripple::toBase58(*accountID);
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

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    ripple::uint256 cursor;
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        if (!cursor.parseHex(request.at("marker").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains("type"))
    {
        if (!request.at("type").is_string())
            return Status{Error::rpcINVALID_PARAMS, "typeNotString"};

        std::string typeAsString = request.at("type").as_string().c_str();
        if (types.find(typeAsString) == types.end())
            return Status{Error::rpcINVALID_PARAMS, "typeInvalid"};

        objectType = types[typeAsString];
    }

    response["account"] = ripple::to_string(*accountID);
    response["account_objects"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects = response.at("account_objects").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (!objectType || objectType == sle.getType())
        {
            if (limit-- == 0)
            {
                return false;
            }

            jsonObjects.push_back(toJson(sle));
        }

        return true;
    };

    auto nextCursor = traverseOwnedNodes(
        *context.backend,
        *accountID,
        lgrInfo.seq,
        cursor,
        context.yield,
        addToResponse);

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    if (nextCursor)
        response["marker"] = ripple::strHex(*nextCursor);

    return response;
}

}  // namespace RPC
