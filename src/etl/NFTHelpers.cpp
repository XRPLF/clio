#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"

#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace etl {

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNftokenModifyData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    auto const tokenID = sttx.getFieldH256(xrpl::sfNFTokenID);
    // note: sfURI is optional, if it is absent, we will update the uri as empty string
    return {
        {NFTTransactionsData(
            sttx.getFieldH256(xrpl::sfNFTokenID), txMeta, sttx.getTransactionID()
        )},
        NFTsData(tokenID, txMeta, sttx.getFieldVL(xrpl::sfURI))
    };
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenMintData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    // To find the minted token ID, we put all tokenIDs referenced in the
    // metadata from prior to the tx application into one vector, then all
    // tokenIDs referenced in the metadata from after the tx application into
    // another, then find the one tokenID that was added by this tx
    // application.
    std::vector<xrpl::uint256> prevIDs;
    std::vector<xrpl::uint256> finalIDs;

    // The owner is not necessarily the issuer, if using authorized minter
    // flow. Determine owner from the ledger object ID of the NFTokenPages
    // that were changed.
    std::optional<xrpl::AccountID> owner;

    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_PAGE)
            continue;

        if (!owner)
            owner = xrpl::AccountID::fromVoid(node.getFieldH256(xrpl::sfLedgerIndex).data());

        if (node.getFName() == xrpl::sfCreatedNode) {
            xrpl::STArray const& toAddNFTs = node.peekAtField(xrpl::sfNewFields)
                                                 .downcast<xrpl::STObject>()
                                                 .getFieldArray(xrpl::sfNFTokens);
            std::ranges::transform(
                toAddNFTs,

                std::back_inserter(finalIDs),
                [](xrpl::STObject const& nft) { return nft.getFieldH256(xrpl::sfNFTokenID); }
            );
        }
        // Else it's modified, as there should never be a deleted NFToken page
        // as a result of a mint.
        else {
            // When a mint results in splitting an existing page,
            // it results in a created page and a modified node. Sometimes,
            // the created node needs to be linked to a third page, resulting
            // in modifying that third page's PreviousPageMin or NextPageMin
            // field changing, but no NFTs within that page changing. In this
            // case, there will be no previous NFTs and we need to skip.
            // However, there will always be NFTs listed in the final fields,
            // as rippled outputs all fields in final fields even if they were
            // not changed.
            xrpl::STObject const& previousFields =
                node.peekAtField(xrpl::sfPreviousFields).downcast<xrpl::STObject>();
            if (!previousFields.isFieldPresent(xrpl::sfNFTokens))
                continue;

            xrpl::STArray const& toAddNFTs = previousFields.getFieldArray(xrpl::sfNFTokens);
            std::ranges::transform(
                toAddNFTs,

                std::back_inserter(prevIDs),
                [](xrpl::STObject const& nft) { return nft.getFieldH256(xrpl::sfNFTokenID); }
            );

            xrpl::STArray const& toAddFinalNFTs = node.peekAtField(xrpl::sfFinalFields)
                                                      .downcast<xrpl::STObject>()
                                                      .getFieldArray(xrpl::sfNFTokens);
            std::ranges::transform(
                toAddFinalNFTs,

                std::back_inserter(finalIDs),
                [](xrpl::STObject const& nft) { return nft.getFieldH256(xrpl::sfNFTokenID); }
            );
        }
    }

    std::ranges::sort(finalIDs);
    std::ranges::sort(prevIDs);

    // Find the first NFT ID that doesn't match.  We're looking for an
    // added NFT, so the one we want will be the mismatch in finalIDs.
    auto const [finalMismatch, prevMismatch] = std::ranges::mismatch(finalIDs, prevIDs);

    // There should always be a difference so the returned finalIDs
    // iterator should never be end().  But better safe than sorry.
    if (finalIDs.size() != prevIDs.size() + 1 || finalMismatch == finalIDs.end() || !owner) {
        throw std::runtime_error(
            fmt::format(" - unexpected NFTokenMint data in tx {}", strHex(sttx.getTransactionID()))
        );
    }

    return {
        {NFTTransactionsData(*finalMismatch, txMeta, sttx.getTransactionID())},
        NFTsData(*finalMismatch, *owner, sttx.getFieldVL(xrpl::sfURI), txMeta)
    };
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenBurnData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    xrpl::uint256 const tokenID = sttx.getFieldH256(xrpl::sfNFTokenID);
    std::vector<NFTTransactionsData> const txs = {
        NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())
    };

    // Determine who owned the token when it was burned by finding an
    // NFTokenPage that was deleted or modified that contains this
    // tokenID.
    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_PAGE ||
            node.getFName() == xrpl::sfCreatedNode)
            continue;

        // NFT burn can result in an NFTokenPage being modified to no longer
        // include the target, or an NFTokenPage being deleted. If this is
        // modified, we want to look for the target in the fields prior to
        // modification. If deleted, it's possible that the page was
        // modified to remove the target NFT prior to the entire page being
        // deleted. In this case, we need to look in the PreviousFields.
        // Otherwise, the page was not modified prior to deleting and we
        // need to look in the FinalFields.
        std::optional<xrpl::STArray> prevNFTs;

        if (node.isFieldPresent(xrpl::sfPreviousFields)) {
            xrpl::STObject const& previousFields =
                node.peekAtField(xrpl::sfPreviousFields).downcast<xrpl::STObject>();
            if (previousFields.isFieldPresent(xrpl::sfNFTokens))
                prevNFTs = previousFields.getFieldArray(xrpl::sfNFTokens);
        } else if (node.getFName() == xrpl::sfDeletedNode) {
            prevNFTs = node.peekAtField(xrpl::sfFinalFields)
                           .downcast<xrpl::STObject>()
                           .getFieldArray(xrpl::sfNFTokens);
        }

        if (!prevNFTs)
            continue;

        auto const nft = std::find_if(
            prevNFTs->begin(), prevNFTs->end(), [&tokenID](xrpl::STObject const& candidate) {
                return candidate.getFieldH256(xrpl::sfNFTokenID) == tokenID;
            }
        );
        if (nft != prevNFTs->end()) {
            return std::make_pair(
                txs,
                NFTsData(
                    tokenID,
                    xrpl::AccountID::fromVoid(node.getFieldH256(xrpl::sfLedgerIndex).data()),
                    txMeta,
                    true
                )
            );
        }
    }

    std::stringstream msg;
    msg << " - could not determine owner at burntime for tx " << sttx.getTransactionID();
    throw std::runtime_error(msg.str());
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenAcceptOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    // If we have the buy offer from this tx, we can determine the owner
    // more easily by just looking at the owner of the accepted NFTokenOffer
    // object.
    if (sttx.isFieldPresent(xrpl::sfNFTokenBuyOffer)) {
        auto const affectedBuyOffer = std::find_if(
            txMeta.getNodes().begin(),
            txMeta.getNodes().end(),
            [&sttx](xrpl::STObject const& node) {
                return node.getFieldH256(xrpl::sfLedgerIndex) ==
                    sttx.getFieldH256(xrpl::sfNFTokenBuyOffer);
            }
        );
        if (affectedBuyOffer == txMeta.getNodes().end()) {
            std::stringstream msg;
            msg << " - unexpected NFTokenAcceptOffer data in tx " << sttx.getTransactionID();
            throw std::runtime_error(msg.str());
        }

        xrpl::uint256 const tokenID = affectedBuyOffer->peekAtField(xrpl::sfFinalFields)
                                          .downcast<xrpl::STObject>()
                                          .getFieldH256(xrpl::sfNFTokenID);

        xrpl::AccountID const owner = affectedBuyOffer->peekAtField(xrpl::sfFinalFields)
                                          .downcast<xrpl::STObject>()
                                          .getAccountID(xrpl::sfOwner);
        return {
            {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())},
            NFTsData(tokenID, owner, txMeta, false)
        };
    }

    // Otherwise we have to infer the new owner from the affected nodes.
    auto const affectedSellOffer = std::find_if(
        txMeta.getNodes().begin(), txMeta.getNodes().end(), [&sttx](xrpl::STObject const& node) {
            return node.getFieldH256(xrpl::sfLedgerIndex) ==
                sttx.getFieldH256(xrpl::sfNFTokenSellOffer);
        }
    );
    if (affectedSellOffer == txMeta.getNodes().end()) {
        std::stringstream msg;
        msg << " - unexpected NFTokenAcceptOffer data in tx " << sttx.getTransactionID();
        throw std::runtime_error(msg.str());
    }

    xrpl::uint256 const tokenID = affectedSellOffer->peekAtField(xrpl::sfFinalFields)
                                      .downcast<xrpl::STObject>()
                                      .getFieldH256(xrpl::sfNFTokenID);

    xrpl::AccountID const seller = affectedSellOffer->peekAtField(xrpl::sfFinalFields)
                                       .downcast<xrpl::STObject>()
                                       .getAccountID(xrpl::sfOwner);

    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_PAGE ||
            node.getFName() == xrpl::sfDeletedNode)
            continue;

        xrpl::AccountID const nodeOwner =
            xrpl::AccountID::fromVoid(node.getFieldH256(xrpl::sfLedgerIndex).data());
        if (nodeOwner == seller)
            continue;

        xrpl::STArray const& nfts = [&node] {
            if (node.getFName() == xrpl::sfCreatedNode) {
                return node.peekAtField(xrpl::sfNewFields)
                    .downcast<xrpl::STObject>()
                    .getFieldArray(xrpl::sfNFTokens);
            }
            return node.peekAtField(xrpl::sfFinalFields)
                .downcast<xrpl::STObject>()
                .getFieldArray(xrpl::sfNFTokens);
        }();

        auto const nft = std::ranges::find_if(nfts, [&tokenID](xrpl::STObject const& candidate) {
            return candidate.getFieldH256(xrpl::sfNFTokenID) == tokenID;
        });
        if (nft != nfts.end()) {
            return {
                {NFTTransactionsData(tokenID, txMeta, sttx.getTransactionID())},
                NFTsData(tokenID, nodeOwner, txMeta, false)
            };
        }
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
getNFTokenCancelOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    std::vector<NFTTransactionsData> txs;
    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_OFFER)
            continue;

        xrpl::uint256 const tokenID = node.peekAtField(xrpl::sfFinalFields)
                                          .downcast<xrpl::STObject>()
                                          .getFieldH256(xrpl::sfNFTokenID);
        txs.emplace_back(tokenID, txMeta, sttx.getTransactionID());
    }

    // Deduplicate any transactions based on tokenID
    std::ranges::sort(txs, [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
        return a.tokenID < b.tokenID;
    });
    auto [last, end] =
        std::ranges::unique(txs, [](NFTTransactionsData const& a, NFTTransactionsData const& b) {
            return a.tokenID == b.tokenID;
        });
    txs.erase(last, end);
    return {txs, {}};
}

// This transaction never returns an NFTokensData because it does not
// change the state of an NFT itself.
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCreateOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    return {
        {NFTTransactionsData(
            sttx.getFieldH256(xrpl::sfNFTokenID), txMeta, sttx.getTransactionID()
        )},
        {}
    };
}

std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTDataFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    if (txMeta.getResultTER() != xrpl::tesSUCCESS)
        return {{}, {}};

    switch (sttx.getTxnType()) {
        case xrpl::TxType::ttNFTOKEN_MINT:
            return getNFTokenMintData(txMeta, sttx);

        case xrpl::TxType::ttNFTOKEN_BURN:
            return getNFTokenBurnData(txMeta, sttx);

        case xrpl::TxType::ttNFTOKEN_ACCEPT_OFFER:
            return getNFTokenAcceptOfferData(txMeta, sttx);

        case xrpl::TxType::ttNFTOKEN_CANCEL_OFFER:
            return getNFTokenCancelOfferData(txMeta, sttx);

        case xrpl::TxType::ttNFTOKEN_CREATE_OFFER:
            return getNFTokenCreateOfferData(txMeta, sttx);

        case xrpl::TxType::ttNFTOKEN_MODIFY:
            return getNftokenModifyData(txMeta, sttx);

        default:
            return {{}, {}};
    }
}

std::vector<NFTsData>
getNFTDataFromObj(std::uint32_t const seq, std::string const& key, std::string const& blob)
{
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0020-non-fungible-tokens#tokenpage-id-format
    ASSERT(
        key.size() == xrpl::uint256::size(),
        "The size of the key (token) is expected to fit uint256 exactly"
    );

    auto const sle = xrpl::STLedgerEntry(
        xrpl::SerialIter{blob.data(), blob.size()}, xrpl::uint256::fromVoid(key.data())
    );

    if (sle.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_PAGE)
        return {};

    auto const owner = xrpl::AccountID::fromVoid(key.data());
    std::vector<NFTsData> nfts;

    for (xrpl::STObject const& node : sle.getFieldArray(xrpl::sfNFTokens)) {
        nfts.emplace_back(
            node.getFieldH256(xrpl::sfNFTokenID), seq, owner, node.getFieldVL(xrpl::sfURI)
        );
    }

    return nfts;
}

std::vector<NFTsData>
getUniqueNFTsDatas(std::vector<NFTsData> const& nfts)
{
    std::vector<NFTsData> results = nfts;

    std::ranges::sort(results, [](NFTsData const& a, NFTsData const& b) {
        return a.tokenID == b.tokenID ? a.transactionIndex > b.transactionIndex
                                      : a.tokenID > b.tokenID;
    });

    auto const [last, end] = std::ranges::unique(results, [](NFTsData const& a, NFTsData const& b) {
        return a.tokenID == b.tokenID;
    });
    results.erase(last, end);
    return results;
}

}  // namespace etl
