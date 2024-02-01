//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include "data/Types.h"

#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>

#include <optional>
#include <string_view>

/*
 * Create AccountID object with string
 */
[[nodiscard]] ripple::AccountID
GetAccountIDWithString(std::string_view id);

/**
 * Create AccountID object with string and return its key
 */
[[nodiscard]] ripple::uint256
GetAccountKey(std::string_view id);

/*
 * Gets the account key from an account id
 */
[[nodiscard]] ripple::uint256
GetAccountKey(ripple::AccountID const& acc);

/*
 * Create a simple ledgerInfo object with only hash and seq
 */
[[nodiscard]] ripple::LedgerInfo
CreateLedgerInfo(std::string_view ledgerHash, ripple::LedgerIndex seq, std::optional<uint32_t> age = std::nullopt);

/*
 * Create a FeeSetting ledger object
 */
[[nodiscard]] ripple::STObject
CreateFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
);

/*
 * Create a FeeSetting ledger object and return its blob
 */
[[nodiscard]] ripple::Blob
CreateFeeSettingBlob(uint64_t base, uint32_t reserveInc, uint32_t reserveBase, uint32_t refFeeUnit, uint32_t flag);

/*
 * Create a payment transaction object
 */
[[nodiscard]] ripple::STObject
CreatePaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq
);

[[nodiscard]] ripple::STObject
CreatePaymentTransactionMetaObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int finalBalance1,
    int finalBalance2,
    uint32_t transactionIndex = 0
);

/*
 * Create an account root ledger object
 */
[[nodiscard]] ripple::STObject
CreateAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq,
    uint32_t transferRate = 0
);

/*
 * Create a createoffer treansaction
 * Taker pay is XRP
 * If reverse is true, taker gets is XRP
 */
[[nodiscard]] ripple::STObject
CreateCreateOfferTransactionObject(
    std::string_view accountId,
    int fee,
    uint32_t seq,
    std::string_view currency,
    std::string_view issuer,
    int takerGets,
    int takerPays,
    bool reverse = false
);

/*
 * Return an issue object with given currency and issue account
 */
[[nodiscard]] ripple::Issue
GetIssue(std::string_view currency, std::string_view issuerId);

/*
 * Create a offer change meta data
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int perviousTakerGets,
    int finalTakerPays,
    int perviousTakerPays
);

/*
 * Meta data for adding a offer object
 * finalTakerGets is XRP
 * If reverse is true, finalTakerPays is XRP
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForCreateOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays,
    bool reverse = false
);

/*
 * Meta data for removing a offer object
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForCancelOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays
);

/*
 * Create a owner dir ledger object
 */
[[nodiscard]] ripple::STObject
CreateOwnerDirLedgerObject(std::vector<ripple::uint256> indexes, std::string_view rootIndex);

/*
 * Create a payment channel ledger object
 */
[[nodiscard]] ripple::STObject
CreatePaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq
);

[[nodiscard]] ripple::STObject
CreateRippleStateLedgerObject(
    std::string_view currency,
    std::string_view issuerId,
    int balance,
    std::string_view lowNodeAccountId,
    int lowLimit,
    std::string_view highNodeAccountId,
    int highLimit,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq,
    uint32_t flag = 0
);

[[nodiscard]] ripple::STObject
CreateOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view paysCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view bookDirId
);

[[nodiscard]] ripple::STObject
CreateTicketLedgerObject(std::string_view account, uint32_t sequence);

[[nodiscard]] ripple::STObject
CreateEscrowLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] ripple::STObject
CreateCheckLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] ripple::STObject
CreateDepositPreauthLedgerObject(std::string_view account, std::string_view auth);

[[nodiscard]] data::NFT
CreateNFT(
    std::string_view tokenID,
    std::string_view account,
    ripple::LedgerIndex seq = 1234u,
    ripple::Blob uri = ripple::Blob{'u', 'r', 'i'},
    bool isBurned = false
);

[[nodiscard]] ripple::STObject
CreateNFTBuyOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] ripple::STObject
CreateNFTSellOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] ripple::STObject
CreateSignerLists(std::vector<std::pair<std::string, uint32_t>> const& signers);

[[nodiscard]] ripple::STObject
CreateNFTTokenPage(
    std::vector<std::pair<std::string, std::string>> const& tokens,
    std::optional<ripple::uint256> previousPage
);

[[nodiscard]] data::TransactionAndMetadata
CreateMintNFTTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::string_view nftID
);

[[nodiscard]] data::TransactionAndMetadata
CreateAcceptNFTOfferTxWithMetadata(std::string_view accountId, uint32_t seq, uint32_t fee, std::string_view nftId);

[[nodiscard]] data::TransactionAndMetadata
CreateCancelNFTOffersTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::vector<std::string> const& nftOffers
);

[[nodiscard]] data::TransactionAndMetadata
CreateCreateNFTOfferTxWithMetadata(std::string_view accountId, uint32_t seq, uint32_t fee, std::string_view offerId);

[[nodiscard]] data::TransactionAndMetadata
CreateCreateNFTOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::uint32_t offerPrice,
    std::string_view offerId
);

[[nodiscard]] ripple::STObject
CreateAmendmentsObject(std::vector<ripple::uint256> const& enabledAmendments);

[[nodiscard]] ripple::STObject
CreateAMMObject(
    std::string_view accountId,
    std::string_view assetCurrency,
    std::string_view assetIssuer,
    std::string_view asset2Currency,
    std::string_view asset2Issuer,
    std::string_view lpTokenBalanceIssueCurrency = "03930D02208264E2E40EC1B0C09E4DB96EE197B1",
    uint32_t lpTokenBalanceIssueAmount = 100u,
    uint16_t tradingFee = 5u,
    uint64_t ownerNode = 0u
);

[[nodiscard]] ripple::STObject
CreateBridgeObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

[[nodiscard]] ripple::STObject
CreateChainOwnedClaimIDObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer,
    std::string_view otherChainSource
);

[[nodiscard]] ripple::STObject
CreateChainOwnedCreateAccountClaimID(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

void
AMMAddVoteSlot(ripple::STObject& amm, ripple::AccountID const& accountId, uint16_t tradingFee, uint32_t voteWeight);

void
AMMSetAuctionSlot(
    ripple::STObject& amm,
    ripple::AccountID const& accountId,
    ripple::STAmount price,
    uint16_t discountedFee,
    uint32_t expiration,
    std::vector<ripple::AccountID> const& authAccounts = {}
);

[[nodiscard]] ripple::STObject
CreateDidObject(std::string_view accountId, std::string_view didDoc, std::string_view uri, std::string_view data);

[[nodiscard]] ripple::Currency
CreateLPTCurrency(std::string_view assetCurrency, std::string_view asset2Currency);
