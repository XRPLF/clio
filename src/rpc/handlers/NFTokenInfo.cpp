#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

// {
//   tokenid: <ident>
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }

namespace RPC {

static std::optional<std::string>
getNFTokenURI(Backend::NFToken const& dbResponse)
{
    if (!dbResponse.page.has_value())
    {
        return {};
    }

    ripple::SLE sle{
        ripple::SerialIter{dbResponse.page.value().blob.data(), dbResponse.page.value().blob.size()},
        dbResponse.page.value().key};

    ripple::STArray nfts = sle.getFieldArray(ripple::sfNonFungibleTokens);
    auto nft = std::find_if(
        nfts.begin(),
        nfts.end(),
        [dbResponse](ripple::STObject const& candidate) {
            return candidate.getFieldH256(ripple::sfTokenID) == dbResponse.tokenID;
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
    response["owner"] = ripple::strHex(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;
    response["uri"] = getNFTokenURI(dbResponse.value()).value_or(nullptr);

    std::uint32_t tokenSeq;
    memcpy(&tokenSeq, dbResponse->tokenID.begin() + 56, 4);
    response["token_sequence"] = boost::endian::big_to_native(tokenSeq);

    std::uint32_t tokenTaxon;
    memcpy(&tokenTaxon, dbResponse->tokenID.begin() + 48, 4);
    response["token_taxon"] = boost::endian::big_to_native(tokenTaxon);

    ripple::AccountID issuer = ripple::AccountID::fromVoid(dbResponse->tokenID.data() + 4);
    response["issuer"] = ripple::strHex(issuer);

    std::uint16_t transferFee;
    memcpy(&transferFee, dbResponse->tokenID.begin() + 2, 2);
    response["transfer_fee"] = boost::endian::big_to_native(transferFee);

    std::uint16_t flags;
    memcpy(&flags, dbResponse->tokenID.begin(), 2);
    response["flags"] = boost::endian::big_to_native(flags);

    return response;
}

}  // namespace RPC
