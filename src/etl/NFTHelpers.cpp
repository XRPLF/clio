#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <string>
#include <vector>
#include <set>

namespace etl {
static std::vector<std::reference_wrapper<const ripple::STObject>>
getAffectedPages(ripple::TxMeta const& txMeta, std::string pageType)
{
    std::vector<std::reference_wrapper<const ripple::STObject>> ret;
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        for (ripple::STBase const& field : node)
        {
            if (field.getFName() == ripple::sfLedgerEntryType &&
                field.getText() == pageType)
            {
                ret.push_back(std::cref(node));
                break;
            }
        }
    }
    return ret;
}

static std::set<std::string>
getTokenIDsFromField(ripple::STObject const& fields)
{
    std::set<std::string> ret;
    for (ripple::STBase const& field : fields)
    {
        if (field.getFName() != ripple::sfNonFungibleTokens)
        {
            continue;
        }
        for (ripple::STBase const& subfield : field.downcast<ripple::STArray>())
        {
            if (subfield.getFName() != ripple::sfNonFungibleToken)
            {
                continue;
            }
            for (ripple::STBase const& subsubfield : subfield.downcast<ripple::STObject>())
            {
                if (subsubfield.getFName() == ripple::sfTokenID)
                {
                    ret.insert(subsubfield.getText());
                }
            }
        }
    }
    return ret;
}

static std::string
getNFTokenIDNFTokenMint(ripple::TxMeta const& txMeta)
{
    std::set<std::string> previousIds;
    std::set<std::string> finalIds;
    for (ripple::STBase const& field : getAffectedPages(txMeta, "NFTokenPage").front().get())
    {
        if (field.getFName() == ripple::sfPreviousFields)
        {
            previousIds = getTokenIDsFromField(field.downcast<ripple::STObject>());
        }
        else if (field.getFName() == ripple::sfFinalFields ||
            field.getFName() == ripple::sfNewFields)
        {
            finalIds = getTokenIDsFromField(field.downcast<ripple::STObject>());
        }
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
getNFTokenIDNFTokenBurn(ripple::STTx const& sttx)
{
    for (ripple::STBase const& field : sttx)
    {
        if (field.getFName() == ripple::sfTokenID)
        {
            return field.getText();
        }
    }

    throw std::runtime_error("TokenID not found for NFTokenBurn transaction");
}

static std::string
getNFTokenIDNFTokenAcceptOffer(ripple::TxMeta const& txMeta)
{
    for (ripple::STBase const& field : getAffectedPages(txMeta, "NFTokenOffer").front().get())
    {
        if (field.getFName() != ripple::sfFinalFields)
        {
            continue;
        }
        for (ripple::STBase const& subfield : field.downcast<ripple::STObject>())
        {
            if (subfield.getFName() == ripple::sfTokenID)
            {
                return subfield.getText();
            }
        }
    }

    throw std::runtime_error("TokenID not found for NFTokenAcceptOffer transaction");
}

std::string
getNFTokenID(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    switch (sttx.getTxnType())
    {
        case ripple::TxType::ttNFTOKEN_MINT:
            return getNFTokenIDNFTokenMint(txMeta);

        case ripple::TxType::ttNFTOKEN_BURN:
            return getNFTokenIDNFTokenBurn(sttx);

        case ripple::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getNFTokenIDNFTokenAcceptOffer(txMeta);

        default:
            throw std::runtime_error("Invalid transaction type for NFToken");
   }
}

static std::string
getNFTokenNewOwnerNFTokenBurn(ripple::TxMeta const& txMeta)
{
    for (ripple::STBase const& field : getAffectedPages(txMeta, "NFTokenPage").front().get())
    {
        if (field.getFName() == ripple::sfLedgerIndex)
        {
            return field.getText().substr(0, 40);
        }
    }

    throw std::runtime_error("New owner not found for NFTokenBurn transaction");
}

static std::string
getNFTokenNewOwnerNFTokenAcceptOffer(ripple::TxMeta const& txMeta)
{
    std::optional<std::reference_wrapper<const ripple::STObject>> createdNode;
    std::vector<std::reference_wrapper<const ripple::STObject>> modifiedNodes;
    for (std::reference_wrapper<const ripple::STObject> page : getAffectedPages(txMeta, "NFTokenPage"))
    {
        for (ripple::STBase const& field : page.get())
        {
            // Only created nodes have the NewFields field
            if (field.getFName() == ripple::sfNewFields && !createdNode.has_value())
            {
                createdNode = page;
            }
            // Only modified nodes have the PreviousFields field
            else if (field.getFName() == ripple::sfPreviousFields)
            {
                modifiedNodes.push_back(page);
            }
        }
    }

    // In this case, we can infer the owner's node because there is a
    // single created NFTokenPage, belonging to the new owner (the old owner's
    // NFTokenPage was either modified or deleted).
    if (createdNode.has_value())
    {
        for (ripple::STBase const& field : createdNode.value().get())
        {
            if (field.getFName() == ripple::sfLedgerIndex)
            {
                return field.getText().substr(0, 40);
            }
        }

        throw std::runtime_error("New owner not found for NFTokenAcceptOffer transaction");
    }
        
    // find the one modified node where the nonfungible token length increased
    // from previous to final
    auto ownerNode = std::find_if(
        modifiedNodes.begin(),
        modifiedNodes.end(),
        [](std::reference_wrapper<const ripple::STObject> node)
        {
            int prevLen;
            int finalLen;

            for (ripple::STBase const& field : node.get())
            {
                if (field.getFName() != ripple::sfPreviousFields &&
                    field.getFName() != ripple::sfFinalFields)
                {
                    continue;
                }
                for (ripple::STBase const& subfield : field.downcast<ripple::STObject>())
                {
                    if (subfield.getFName() != ripple::sfNonFungibleTokens)
                    {
                        continue;
                    }
                    if (field.getFName() == ripple::sfPreviousFields)
                    {
                        prevLen = subfield.downcast<ripple::STArray>().size();
                        break;
                    }
                    finalLen = subfield.downcast<ripple::STArray>().size();
                    break;
                }
            }

            return finalLen > prevLen;
          });

    // get the owner from this modified node
    if (ownerNode != std::end(modifiedNodes))
    {
        for (ripple::STBase const& field : (*ownerNode).get())
        {
            if (field.getFName() == ripple::sfLedgerIndex)
            {
                return field.getText().substr(0, 40);
            }
        }
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
            return getNFTokenNewOwnerNFTokenBurn(txMeta);

        case ripple::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getNFTokenNewOwnerNFTokenAcceptOffer(txMeta);

        default:
            throw std::runtime_error("Invalid transaction type for NFToken");
    }
}
}  // namespace etl
