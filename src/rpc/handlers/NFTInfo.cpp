#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

// {
//   tokenid: <ident>
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }

namespace RPC {

std::optional<std::string>
getNFTokenURI(ripple::TxMeta const& txMeta, ripple::uint256 const& tokenID)
{
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) !=
                ripple::ltNFTOKEN_PAGE ||
            node.getFName() == ripple::sfDeletedNode)
            continue;

        ripple::STArray const& nfts = [&node] {
            if (node.getFName() == ripple::sfCreatedNode)
                return node.peekAtField(ripple::sfNewFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            return node.peekAtField(ripple::sfFinalFields)
                .downcast<ripple::STObject>()
                .getFieldArray(ripple::sfNFTokens);
        }();

        auto nft = std::find_if(
            nfts.begin(),
            nfts.end(),
            [&tokenID](ripple::STObject const& candidate) {
                return candidate.getFieldH256(ripple::sfNFTokenID) == tokenID;
            });
        if (nft != nfts.end())
        {
            ripple::Blob uriField = nft->getFieldVL(ripple::sfURI);
            if (std::string const uri =
                    std::string(uriField.begin(), uriField.end());
                uri.size() > 0)
                return uri;
            return {};
        }
    }

    throw std::runtime_error("Unexpected NFT data");
}

Result
doNFTInfo(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("nft_id"))
        return Status{Error::rpcINVALID_PARAMS, "Missing nft_id"};

    auto const& jsonTokenID = request.at("nft_id");
    if (!jsonTokenID.is_string())
        return Status{Error::rpcINVALID_PARAMS, "nft_id is not a string"};

    ripple::uint256 tokenID;
    if (!tokenID.parseHex(jsonTokenID.as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "Malformed nft_id"};

    // We only need to fetch the ledger header because the ledger hash is
    // supposed to be included in the response. The ledger sequence is specified
    // in the request
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;
    ripple::LedgerInfo lgrInfo = std::get<ripple::LedgerInfo>(v);

    std::optional<Backend::NFT> dbResponse =
        context.backend->fetchNFT(tokenID, lgrInfo.seq, context.yield);
    if (!dbResponse)
        return Status{Error::rpcOBJECT_NOT_FOUND, "NFT not found"};

    response["nft_id"] = ripple::strHex(dbResponse->tokenID);
    response["ledger_index"] = dbResponse->ledgerSequence;
    response["owner"] = ripple::toBase58(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;

    response["flags"] = ripple::nft::getFlags(dbResponse->tokenID);
    response["transfer_fee"] = ripple::nft::getTransferFee(dbResponse->tokenID);
    response["issuer"] =
        ripple::toBase58(ripple::nft::getIssuer(dbResponse->tokenID));
    response["nft_taxon"] =
        ripple::nft::toUInt32(ripple::nft::getTaxon(dbResponse->tokenID));
    response["nft_sequence"] = ripple::nft::getSerial(dbResponse->tokenID);

    // Fetch URI from first transaction
    Backend::TransactionsAndCursor dbTxResponse =
        context.backend->fetchNFTTransactions(
            dbResponse->tokenID, 1, true, {}, context.yield);

    if (dbTxResponse.txns.size() != 1)
        return Status{
            Error::rpcINTERNAL,
            "Could not find first transaction for this NFT"};

    auto [_, txMeta] = deserializeTxPlusMeta(
        dbTxResponse.txns.front(), dbTxResponse.txns.front().ledgerSequence);
    std::optional<std::string> uri =
        getNFTokenURI(*txMeta, dbResponse->tokenID);

    // NOTE this cannot use a ternary or value_or because then the
    // expression's type is unclear. We want to explicitly set the `uri` field
    // to null when not present to avoid any confusion.
    if (uri)
        response["uri"] = *uri;
    else
        response["uri"] = nullptr;

    return response;
}

}  // namespace RPC
