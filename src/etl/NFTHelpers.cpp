#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <vector>

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <backend/Types.h>

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenMintData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    // To find the minted token ID, we put all tokenIDs referenced in the
    // metadata from prior to the tx application into one vector, then all
    // tokenIDs referenced in the metadata from after the tx application into
    // another, then find the one tokenID that was added by this tx
    // application.
    std::vector<ripple::uint256> prevIDs;
    std::vector<ripple::uint256> finalIDs;

    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) !=
            ripple::ltNFTOKEN_PAGE)
            continue;

        if (node.getFName() == ripple::sfCreatedNode)
        {
            ripple::STArray const& toAddNFTs =
                node.peekAtField(ripple::sfNewFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddNFTs.begin(),
                toAddNFTs.end(),
                std::back_inserter(finalIDs),
                [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });
        }
        else if (node.getFName() == ripple::sfModifiedNode)
        {
            ripple::STArray const& toAddNFTs =
                node.peekAtField(ripple::sfPreviousFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddNFTs.begin(),
                toAddNFTs.end(),
                std::back_inserter(prevIDs),
                [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });

            ripple::STArray const& toAddFinalNFTs =
                node.peekAtField(ripple::sfFinalFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddFinalNFTs.begin(),
                toAddFinalNFTs.end(),
                std::back_inserter(finalIDs),
                [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });
        }
        else
        {
            ripple::STArray const& toAddNFTs =
                node.peekAtField(ripple::sfFinalFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddNFTs.begin(),
                toAddNFTs.end(),
                std::back_inserter(prevIDs),
                [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });
        }
    }

    std::sort(finalIDs.begin(), finalIDs.end());
    std::sort(prevIDs.begin(), prevIDs.end());
    std::vector<ripple::uint256> tokenIDResult;
    std::set_difference(
        finalIDs.begin(),
        finalIDs.end(),
        prevIDs.begin(),
        prevIDs.end(),
        std::inserter(tokenIDResult, tokenIDResult.begin()));
    if (tokenIDResult.size() == 1)
    {
        return {
            {NFTTransactionsData(
                tokenIDResult.front(), txMeta, sttx.getTransactionID())},
            NFTsData(
                tokenIDResult.front(),
                ripple::nft::getIssuer(tokenIDResult.front()),
                txMeta,
                false,
                true)};
    }

    std::stringstream msg;
    msg << __func__ << " - unexpected NFTokenMint data";
    throw std::runtime_error(msg.str());
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenBurnData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    ripple::uint256 tokenID = sttx.getFieldH256(ripple::sfNFTokenID);
    std::vector<NFTTransactionsData> txs = {
        NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())};

    // Determine who owned the token when it was burned by finding an
    // NFTokenPage that was deleted or modified that contains this
    // tokenID.
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) !=
                ripple::ltNFTOKEN_PAGE ||
            node.getFName() == ripple::sfCreatedNode)
            continue;

        // NFT burn can result in an NFTokenPage being modified to no longer
        // include the target, or an NFTokenPage being deleted. If this is
        // modified, we want to look for the target in the fields prior to
        // modification. If deleted, "FinalFields" is overloaded to mean the
        // state prior to deletion.
        ripple::STArray const& prevNFTs = [&node] {
            if (node.getFName() == ripple::sfModifiedNode)
                return node.peekAtField(ripple::sfPreviousFields)
                    .downcast<ripple::STObject>()
                    .getFieldArray(ripple::sfNFTokens);
            return node.peekAtField(ripple::sfFinalFields)
                .downcast<ripple::STObject>()
                .getFieldArray(ripple::sfNFTokens);
        }();

        auto nft = std::find_if(
            prevNFTs.begin(),
            prevNFTs.end(),
            [&tokenID](ripple::STObject const& candidate) {
                return candidate.getFieldH256(ripple::sfNFTokenID) == tokenID;
            });
        if (nft != prevNFTs.end())
            return std::make_pair(
                txs,
                NFTsData(
                    tokenID,
                    ripple::AccountID::fromVoid(
                        node.getFieldH256(ripple::sfLedgerIndex).data()),
                    txMeta,
                    true,
                    false));
    }

    std::stringstream msg;
    msg << __func__ << " - could not determine owner at burntime";
    throw std::runtime_error(msg.str());
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenAcceptOfferData(
    ripple::TxMeta const& txMeta,
    ripple::STTx const& sttx)
{
    // If we have the buy offer from this tx, we can determine the owner
    // more easily by just looking at the owner of the accepted NFTokenOffer
    // object.
    if (sttx.isFieldPresent(ripple::sfNFTokenBuyOffer))
    {
        auto affectedBuyOffer = std::find_if(
            txMeta.getNodes().begin(),
            txMeta.getNodes().end(),
            [](ripple::STObject const& node) {
                return node.getFieldU16(ripple::sfLedgerEntryType) ==
                    ripple::ltNFTOKEN_OFFER &&
                    !node.isFlag(ripple::lsfSellNFToken);
            });
        if (affectedBuyOffer == txMeta.getNodes().end())
            throw std::runtime_error("Unexpected NFTokenAcceptOffer data");

        ripple::uint256 tokenID = (*affectedBuyOffer)
                                      .peekAtField(ripple::sfFinalFields)
                                      .downcast<ripple::STObject>()
                                      .getFieldH256(ripple::sfNFTokenID);

        ripple::AccountID owner = (*affectedBuyOffer)
                                      .peekAtField(ripple::sfFinalFields)
                                      .downcast<ripple::STObject>()
                                      .getAccountID(ripple::sfOwner);
        return {
            {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())},
            NFTsData(tokenID, owner, txMeta, false, false)};
    }

    // Otherwise we have to infer the new owner from the affected nodes.
    auto affectedSellOffer = std::find_if(
        txMeta.getNodes().begin(),
        txMeta.getNodes().end(),
        [](ripple::STObject const& node) {
            return node.getFieldU16(ripple::sfLedgerEntryType) ==
                ripple::ltNFTOKEN_OFFER &&
                node.isFlag(ripple::lsfSellNFToken);
        });
    if (affectedSellOffer == txMeta.getNodes().end())
        throw std::runtime_error("Unexpected NFTokenAcceptOffer data");

    ripple::uint256 tokenID = (*affectedSellOffer)
                                  .peekAtField(ripple::sfFinalFields)
                                  .downcast<ripple::STObject>()
                                  .getFieldH256(ripple::sfNFTokenID);

    ripple::AccountID seller = (*affectedSellOffer)
                                   .peekAtField(ripple::sfFinalFields)
                                   .downcast<ripple::STObject>()
                                   .getAccountID(ripple::sfOwner);

    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) !=
                ripple::ltNFTOKEN_PAGE ||
            node.getFName() == ripple::sfDeletedNode)
            continue;

        ripple::AccountID nodeOwner = ripple::AccountID::fromVoid(
            node.getFieldH256(ripple::sfLedgerIndex).data());
        if (nodeOwner == seller)
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
            return {
                {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())},
                NFTsData(tokenID, nodeOwner, txMeta, false, false)};
    }

    std::stringstream msg;
    msg << __func__ << " - unexpected NFTokenAcceptOffer tx data";
    throw std::runtime_error(msg.str());
}

// This is the only transaction where there can be more than 1 element in
// the returned vector, because you can cancel multiple offers in one
// transaction using this feature. This transaction also never returns an
// NFTsData because it does not change the state of an NFT itself.
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCancelOfferData(
    ripple::TxMeta const& txMeta,
    ripple::STTx const& sttx)
{
    std::vector<NFTTransactionsData> txs;
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) !=
            ripple::ltNFTOKEN_OFFER)
            continue;

        ripple::uint256 tokenID = node.peekAtField(ripple::sfFinalFields)
                                      .downcast<ripple::STObject>()
                                      .getFieldH256(ripple::sfNFTokenID);
        txs.emplace_back(tokenID, txMeta, sttx.getTransactionID());
    }

    // Deduplicate any transactions based on tokenID/txIdx combo. Can't just
    // use txIdx because in this case one tx can cancel offers for several
    // NFTs.
    std::sort(
        txs.begin(),
        txs.end(),
        [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
            return a.tokenID < b.tokenID &&
                a.transactionIndex < b.transactionIndex;
        });
    auto last = std::unique(
        txs.begin(),
        txs.end(),
        [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
            return a.tokenID == b.tokenID &&
                a.transactionIndex == b.transactionIndex;
        });
    txs.erase(last, txs.end());
    return {txs, {}};
}

// This transaction never returns an NFTokensData because it does not
// change the state of an NFT itself.
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCreateOfferData(
    ripple::TxMeta const& txMeta,
    ripple::STTx const& sttx)
{
    return {
        {NFTTransactionsData(
            sttx.getFieldH256(ripple::sfNFTokenID),
            txMeta,
            sttx.getTransactionID())},
        {}};
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    if (txMeta.getResultTER() != ripple::tesSUCCESS)
        return {{}, {}};

    switch (sttx.getTxnType())
    {
        case ripple::TxType::ttNFTOKEN_MINT:
            return getNFTokenMintData(txMeta, sttx);

        case ripple::TxType::ttNFTOKEN_BURN:
            return getNFTokenBurnData(txMeta, sttx);

        case ripple::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getNFTokenAcceptOfferData(txMeta, sttx);

        case ripple::TxType::ttNFTOKEN_CANCEL_OFFER:
            return getNFTokenCancelOfferData(txMeta, sttx);

        case ripple::TxType::ttNFTOKEN_CREATE_OFFER:
            return getNFTokenCreateOfferData(txMeta, sttx);

        default:
            return {{}, {}};
    }
}
