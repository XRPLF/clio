#include <boost/json.hpp>
#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

// {
//   tokenid: <ident>
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }

namespace RPC {

static std::string
getNFTokenURI(
    Backend::LedgerObject const& dbResponse,
    ripple::uint256 const& tokenID)
{
    ripple::SLE sle{
        ripple::SerialIter{dbResponse.blob.data(), dbResponse.blob.size()},
        dbResponse.key};
    if (sle.getType() != ripple::ltNFTOKEN_PAGE)
    {
        std::stringstream msg;
        msg << __func__ << " - received unexpected object type " << sle.getType();
        throw std::runtime_error(msg.str());
    }

    ripple::STArray nfts = sle.getFieldArray(ripple::sfNonFungibleTokens);
    auto nft = std::find_if(
        nfts.begin(),
        nfts.end(),
        [tokenID](ripple::STObject const& candidate) {
            return candidate.getFieldH256(ripple::sfTokenID) == tokenID;
        });
    if (nft == nfts.end())
    {
        throw std::runtime_error("Token not found in expected page");
    }

    ripple::Blob uriField = (*nft).getFieldVL(ripple::sfURI);
    return std::string(uriField.begin(), uriField.end());
}

Result
doNFTokenInfo(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("tokenid"))
    {
        return Status{Error::rpcINVALID_PARAMS};
    }
    ripple::uint256 tokenID;
    if (!tokenID.parseHex(request.at("tokenid").as_string().c_str()))
    {
        return Status{Error::rpcINVALID_PARAMS};
    }

    // We only need to fetch the ledger header because the ledger hash is
    // supposed to be included in the response. The ledger sequence is specified
    // in the request
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
    {
        return *status;
    }
    ripple::LedgerInfo lgrInfo = std::get<ripple::LedgerInfo>(v);

    std::optional<Backend::NFToken> dbResponse = context.backend->fetchNFToken(tokenID, lgrInfo.seq);
    if (!dbResponse)
    {
        return Status{Error::rpcOBJECT_NOT_FOUND};
    }

    response["tokenid"] = ripple::strHex(dbResponse->tokenID);
    response["ledger_index"] = dbResponse->ledgerSequence;
    response["owner"] = ripple::toBase58(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;

    response["flags"] = ripple::nft::getFlags(tokenID);
    response["transfer_fee"] = ripple::nft::getTransferFee(tokenID);
    response["issuer"] = ripple::toBase58(ripple::nft::getIssuer(tokenID));
    response["token_taxon"] = ripple::nft::getTaxon(tokenID);
    response["token_sequence"] = ripple::nft::getSerial(tokenID);

    // If the token is burned we will not find it at this ledger sequence (of
    // course) so just stop.
    if (dbResponse->isBurned)
    {
        return response;
    }

    // Now determine the ledger index of the NFTokenPage.
    ripple::Keylet const base = ripple::keylet::nftpage_min(dbResponse->owner);
    ripple::Keylet const min = ripple::keylet::nftpage(base, tokenID);
    ripple::Keylet const max = ripple::keylet::nftpage_max(dbResponse->owner);

    std::optional<Backend::LedgerObject> dbPageResponse = context.backend->fetchNFTokenPage(
        min.key,
        max.key,
        dbResponse->ledgerSequence);

    if (!dbPageResponse.has_value())
    {
        std::stringstream msg;
        msg << __func__ << " - NFTokenPage object not found for token "
            << ripple::strHex(tokenID);
        throw std::runtime_error(msg.str());
    }
    
    response["uri"] = getNFTokenURI(*dbPageResponse, dbResponse->tokenID);
    return response;
}

}  // namespace RPC
