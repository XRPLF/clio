#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <string>
#include <vector>
#include <set>

namespace etl {
static std::vector<std::reference_wrapper<const ripple::STObject>>
getAffectedPages(ripple::TxMeta const& txMeta, ripple::LedgerEntryType pageType)
{
    std::vector<std::reference_wrapper<const ripple::STObject>> ret;
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) == pageType)
        {
            ret.push_back(std::cref(node));
        }
    }
    return ret;
}

static std::set<std::string>
getTokenIDsFromField(ripple::STObject const& fields)
{
    std::set<std::string> tokenIDs;
    for (ripple::STObject const& nft : fields.getFieldArray(ripple::sfNonFungibleTokens))
    {
        ripple::uint256 tokenID = nft.getFieldH256(ripple::sfTokenID);
        tokenIDs.insert(ripple::strHex(tokenID.begin(), tokenID.end()));
    }
    return tokenIDs;
}

static std::string
getTokenIDNFTokenMint(ripple::TxMeta const& txMeta)
{
    ripple::STObject const& affectedPage = getAffectedPages(
        txMeta,
        ripple::ltNFTOKEN_PAGE).front().get();

    std::set<std::string> previousIds;
    std::set<std::string> finalIds;

    if (affectedPage.getFName() == ripple::sfCreatedNode)
    {
        finalIds = getTokenIDsFromField(
            affectedPage.peekAtField(
              ripple::sfNewFields).downcast<ripple::STObject>());
    }
    else
    {
        previousIds = getTokenIDsFromField(
            affectedPage.peekAtField(
              ripple::sfPreviousFields).downcast<ripple::STObject>());
        finalIds = getTokenIDsFromField(
            affectedPage.peekAtField(
              ripple::sfFinalFields).downcast<ripple::STObject>());
    }

    std::set<std::string> result;
    std::set_difference(
        finalIds.begin(),
        finalIds.end(),
        previousIds.begin(),
        previousIds.end(),
        std::inserter(result, result.begin()));
    return *(result.begin());
}

static std::string
getTokenIDNFTokenBurn(ripple::STTx const& sttx)
{
    ripple::uint256 tokenID = sttx.getFieldH256(ripple::sfTokenID);
    return ripple::strHex(tokenID.begin(), tokenID.end());
}

static std::string
getTokenIDNFTokenAcceptOffer(ripple::TxMeta const& txMeta)
{
    ripple::uint256 tokenID = getAffectedPages(
        txMeta,
        ripple::ltNFTOKEN_OFFER).front().get().peekAtField(
          ripple::sfFinalFields).downcast<ripple::STObject>().getFieldH256(ripple::sfTokenID);
    return ripple::strHex(tokenID.begin(), tokenID.end());
}

std::string
getNFTokenID(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    switch (sttx.getTxnType())
    {
        case ripple::TxType::ttNFTOKEN_MINT:
            return getTokenIDNFTokenMint(txMeta);

        case ripple::TxType::ttNFTOKEN_BURN:
            return getTokenIDNFTokenBurn(sttx);

        case ripple::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getTokenIDNFTokenAcceptOffer(txMeta);

        default:
            throw std::runtime_error("Invalid transaction type for NFToken");
   }
}

static std::string
getNewOwnerNFTokenBurn(ripple::TxMeta const& txMeta)
{
    ripple::uint256 ledgerIndex = getAffectedPages(
        txMeta,
        ripple::ltNFTOKEN_PAGE).front().get().getFieldH256(ripple::sfLedgerIndex);
    return ripple::strHex(ledgerIndex.begin(), ledgerIndex.end()).substr(0, 40);
}

static std::string
getNewOwnerNFTokenAcceptOffer(ripple::TxMeta const& txMeta)
{
    std::vector<std::reference_wrapper<const ripple::STObject>> modifiedNodes;
    std::vector<std::reference_wrapper<const ripple::STObject>> affectedPages = getAffectedPages(
        txMeta,
        ripple::ltNFTOKEN_PAGE);
    for (std::reference_wrapper<const ripple::STObject> page : affectedPages)
    {
        if (page.get().getFName() == ripple::sfCreatedNode)
        {
            // In this case, we can infer the owner's node because there is a
            // single created NFTokenPage, belonging to the new owner (the old owner's
            // NFTokenPage was either modified or deleted).
            ripple::uint256 ledgerIndex = page.get().getFieldH256(ripple::sfLedgerIndex);
            return ripple::strHex(ledgerIndex.begin(), ledgerIndex.end()).substr(0, 40);
        }
        if (page.get().getFName() == ripple::sfModifiedNode)
        {
            modifiedNodes.push_back(page);
        }
    }
        
    // find the one modified node where the nonfungible token length increased
    // from previous to final
    auto ownerNode = std::find_if(
        modifiedNodes.begin(),
        modifiedNodes.end(),
        [](std::reference_wrapper<const ripple::STObject> node)
        {
            int prevLen = node.get().peekAtField(
                ripple::sfPreviousFields).downcast<ripple::STObject>().getFieldArray(
                  ripple::sfNonFungibleTokens).size();

            int finalLen = node.get().peekAtField(
                ripple::sfFinalFields).downcast<ripple::STObject>().getFieldArray(
                  ripple::sfNonFungibleTokens).size();

            return finalLen > prevLen;
          });

    // get the owner from this modified node
    if (ownerNode != std::end(modifiedNodes))
    {
        ripple::uint256 ledgerIndex = (*ownerNode).get().getFieldH256(ripple::sfLedgerIndex);
        return ripple::strHex(ledgerIndex.begin(), ledgerIndex.end()).substr(0, 40);
    }

    throw std::runtime_error("New owner not found for NFTokenAcceptOffer transaction");
}

std::optional<std::string>
getNFTokenNewOwner(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    switch (sttx.getTxnType())
    {
        case ripple::TxType::ttNFTOKEN_MINT:
            // the owner is the issuer when minted so there is no new owner
            return {};

        case ripple::TxType::ttNFTOKEN_BURN:
            return getNewOwnerNFTokenBurn(txMeta);

        case ripple::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getNewOwnerNFTokenAcceptOffer(txMeta);

        default:
            throw std::runtime_error("Invalid transaction type for NFToken");
    }
}
}  // namespace etl
