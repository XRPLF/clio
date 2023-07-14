/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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

    // The owner is not necessarily the issuer, if using authorized minter
    // flow. Determine owner from the ledger object ID of the NFTokenPages
    // that were changed.
    std::optional<ripple::AccountID> owner;

    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltNFTOKEN_PAGE)
            continue;

        if (!owner)
            owner = ripple::AccountID::fromVoid(node.getFieldH256(ripple::sfLedgerIndex).data());

        if (node.getFName() == ripple::sfCreatedNode)
        {
            ripple::STArray const& toAddNFTs =
                node.peekAtField(ripple::sfNewFields).downcast<ripple::STObject>().getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddNFTs.begin(), toAddNFTs.end(), std::back_inserter(finalIDs), [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });
        }
        // Else it's modified, as there should never be a deleted NFToken page
        // as a result of a mint.
        else
        {
            // When a mint results in splitting an existing page,
            // it results in a created page and a modified node. Sometimes,
            // the created node needs to be linked to a third page, resulting
            // in modifying that third page's PreviousPageMin or NextPageMin
            // field changing, but no NFTs within that page changing. In this
            // case, there will be no previous NFTs and we need to skip.
            // However, there will always be NFTs listed in the final fields,
            // as rippled outputs all fields in final fields even if they were
            // not changed.
            ripple::STObject const& previousFields =
                node.peekAtField(ripple::sfPreviousFields).downcast<ripple::STObject>();
            if (!previousFields.isFieldPresent(ripple::sfNFTokens))
                continue;

            ripple::STArray const& toAddNFTs = previousFields.getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddNFTs.begin(), toAddNFTs.end(), std::back_inserter(prevIDs), [](ripple::STObject const& nft) {
                    return nft.getFieldH256(ripple::sfNFTokenID);
                });

            ripple::STArray const& toAddFinalNFTs =
                node.peekAtField(ripple::sfFinalFields).downcast<ripple::STObject>().getFieldArray(ripple::sfNFTokens);
            std::transform(
                toAddFinalNFTs.begin(),
                toAddFinalNFTs.end(),
                std::back_inserter(finalIDs),
                [](ripple::STObject const& nft) { return nft.getFieldH256(ripple::sfNFTokenID); });
        }
    }

    std::sort(finalIDs.begin(), finalIDs.end());
    std::sort(prevIDs.begin(), prevIDs.end());

    // Find the first NFT ID that doesn't match.  We're looking for an
    // added NFT, so the one we want will be the mismatch in finalIDs.
    auto const diff = std::mismatch(finalIDs.begin(), finalIDs.end(), prevIDs.begin(), prevIDs.end());

    // There should always be a difference so the returned finalIDs
    // iterator should never be end().  But better safe than sorry.
    if (finalIDs.size() != prevIDs.size() + 1 || diff.first == finalIDs.end() || !owner)
    {
        std::stringstream msg;
        msg << " - unexpected NFTokenMint data in tx " << sttx.getTransactionID();
        throw std::runtime_error(msg.str());
    }

    return {
        {NFTTransactionsData(*diff.first, txMeta, sttx.getTransactionID())},
        NFTsData(*diff.first, *owner, sttx.getFieldVL(ripple::sfURI), txMeta)};
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenBurnData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    ripple::uint256 const tokenID = sttx.getFieldH256(ripple::sfNFTokenID);
    std::vector<NFTTransactionsData> const txs = {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())};

    // Determine who owned the token when it was burned by finding an
    // NFTokenPage that was deleted or modified that contains this
    // tokenID.
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltNFTOKEN_PAGE ||
            node.getFName() == ripple::sfCreatedNode)
            continue;

        // NFT burn can result in an NFTokenPage being modified to no longer
        // include the target, or an NFTokenPage being deleted. If this is
        // modified, we want to look for the target in the fields prior to
        // modification. If deleted, it's possible that the page was
        // modified to remove the target NFT prior to the entire page being
        // deleted. In this case, we need to look in the PreviousFields.
        // Otherwise, the page was not modified prior to deleting and we
        // need to look in the FinalFields.
        std::optional<ripple::STArray> prevNFTs;

        if (node.isFieldPresent(ripple::sfPreviousFields))
        {
            ripple::STObject const& previousFields =
                node.peekAtField(ripple::sfPreviousFields).downcast<ripple::STObject>();
            if (previousFields.isFieldPresent(ripple::sfNFTokens))
                prevNFTs = previousFields.getFieldArray(ripple::sfNFTokens);
        }
        else if (!prevNFTs && node.getFName() == ripple::sfDeletedNode)
            prevNFTs =
                node.peekAtField(ripple::sfFinalFields).downcast<ripple::STObject>().getFieldArray(ripple::sfNFTokens);

        if (!prevNFTs)
            continue;

        auto const nft =
            std::find_if(prevNFTs->begin(), prevNFTs->end(), [&tokenID](ripple::STObject const& candidate) {
                return candidate.getFieldH256(ripple::sfNFTokenID) == tokenID;
            });
        if (nft != prevNFTs->end())
            return std::make_pair(
                txs,
                NFTsData(
                    tokenID,
                    ripple::AccountID::fromVoid(node.getFieldH256(ripple::sfLedgerIndex).data()),
                    txMeta,
                    true));
    }

    std::stringstream msg;
    msg << " - could not determine owner at burntime for tx " << sttx.getTransactionID();
    throw std::runtime_error(msg.str());
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenAcceptOfferData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    // If we have the buy offer from this tx, we can determine the owner
    // more easily by just looking at the owner of the accepted NFTokenOffer
    // object.
    if (sttx.isFieldPresent(ripple::sfNFTokenBuyOffer))
    {
        auto const affectedBuyOffer =
            std::find_if(txMeta.getNodes().begin(), txMeta.getNodes().end(), [&sttx](ripple::STObject const& node) {
                return node.getFieldH256(ripple::sfLedgerIndex) == sttx.getFieldH256(ripple::sfNFTokenBuyOffer);
            });
        if (affectedBuyOffer == txMeta.getNodes().end())
        {
            std::stringstream msg;
            msg << " - unexpected NFTokenAcceptOffer data in tx " << sttx.getTransactionID();
            throw std::runtime_error(msg.str());
        }

        ripple::uint256 const tokenID = affectedBuyOffer->peekAtField(ripple::sfFinalFields)
                                            .downcast<ripple::STObject>()
                                            .getFieldH256(ripple::sfNFTokenID);

        ripple::AccountID const owner = affectedBuyOffer->peekAtField(ripple::sfFinalFields)
                                            .downcast<ripple::STObject>()
                                            .getAccountID(ripple::sfOwner);
        return {
            {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())}, NFTsData(tokenID, owner, txMeta, false)};
    }

    // Otherwise we have to infer the new owner from the affected nodes.
    auto const affectedSellOffer =
        std::find_if(txMeta.getNodes().begin(), txMeta.getNodes().end(), [&sttx](ripple::STObject const& node) {
            return node.getFieldH256(ripple::sfLedgerIndex) == sttx.getFieldH256(ripple::sfNFTokenSellOffer);
        });
    if (affectedSellOffer == txMeta.getNodes().end())
    {
        std::stringstream msg;
        msg << " - unexpected NFTokenAcceptOffer data in tx " << sttx.getTransactionID();
        throw std::runtime_error(msg.str());
    }

    ripple::uint256 const tokenID = affectedSellOffer->peekAtField(ripple::sfFinalFields)
                                        .downcast<ripple::STObject>()
                                        .getFieldH256(ripple::sfNFTokenID);

    ripple::AccountID const seller = affectedSellOffer->peekAtField(ripple::sfFinalFields)
                                         .downcast<ripple::STObject>()
                                         .getAccountID(ripple::sfOwner);

    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltNFTOKEN_PAGE ||
            node.getFName() == ripple::sfDeletedNode)
            continue;

        ripple::AccountID const nodeOwner =
            ripple::AccountID::fromVoid(node.getFieldH256(ripple::sfLedgerIndex).data());
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

        auto const nft = std::find_if(nfts.begin(), nfts.end(), [&tokenID](ripple::STObject const& candidate) {
            return candidate.getFieldH256(ripple::sfNFTokenID) == tokenID;
        });
        if (nft != nfts.end())
            return {
                {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())},
                NFTsData(tokenID, nodeOwner, txMeta, false)};
    }

    std::stringstream msg;
    msg << " - unexpected NFTokenAcceptOffer data in tx " << sttx.getTransactionID();
    throw std::runtime_error(msg.str());
}

// This is the only transaction where there can be more than 1 element in
// the returned vector, because you can cancel multiple offers in one
// transaction using this feature. This transaction also never returns an
// NFTsData because it does not change the state of an NFT itself.
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCancelOfferData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    std::vector<NFTTransactionsData> txs;
    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltNFTOKEN_OFFER)
            continue;

        ripple::uint256 const tokenID =
            node.peekAtField(ripple::sfFinalFields).downcast<ripple::STObject>().getFieldH256(ripple::sfNFTokenID);
        txs.emplace_back(tokenID, txMeta, sttx.getTransactionID());
    }

    // Deduplicate any transactions based on tokenID/txIdx combo. Can't just
    // use txIdx because in this case one tx can cancel offers for several
    // NFTs.
    std::sort(txs.begin(), txs.end(), [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
        return a.tokenID < b.tokenID && a.transactionIndex < b.transactionIndex;
    });
    auto last = std::unique(txs.begin(), txs.end(), [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
        return a.tokenID == b.tokenID && a.transactionIndex == b.transactionIndex;
    });
    txs.erase(last, txs.end());
    return {txs, {}};
}

// This transaction never returns an NFTokensData because it does not
// change the state of an NFT itself.
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCreateOfferData(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    return {{NFTTransactionsData(sttx.getFieldH256(ripple::sfNFTokenID), txMeta, sttx.getTransactionID())}, {}};
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTDataFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
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

std::vector<NFTsData>
getNFTDataFromObj(std::uint32_t const seq, std::string const& key, std::string const& blob)
{
    std::vector<NFTsData> nfts;
    ripple::STLedgerEntry const sle =
        ripple::STLedgerEntry(ripple::SerialIter{blob.data(), blob.size()}, ripple::uint256::fromVoid(key.data()));

    if (sle.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltNFTOKEN_PAGE)
        return nfts;

    auto const owner = ripple::AccountID::fromVoid(key.data());
    for (ripple::STObject const& node : sle.getFieldArray(ripple::sfNFTokens))
        nfts.emplace_back(node.getFieldH256(ripple::sfNFTokenID), seq, owner, node.getFieldVL(ripple::sfURI));

    return nfts;
}
