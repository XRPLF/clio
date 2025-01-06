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

#include "data/Types.hpp"

#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/*
 * Create AccountID object with string
 */
[[nodiscard]] ripple::AccountID
getAccountIdWithString(std::string_view id);

/**
 * Create AccountID object with string and return its key
 */
[[nodiscard]] ripple::uint256
getAccountKey(std::string_view id);

/*
 * Gets the account key from an account id
 */
[[nodiscard]] ripple::uint256
getAccountKey(ripple::AccountID const& acc);

/*
 * Create a simple ledgerHeader object with only hash and seq
 */
[[nodiscard]] ripple::LedgerHeader
createLedgerHeader(std::string_view ledgerHash, ripple::LedgerIndex seq, std::optional<uint32_t> age = std::nullopt);

/*
 * Create a simple ledgerHeader object with hash, seq and unix timestamp
 */
[[nodiscard]] ripple::LedgerHeader
createLedgerHeaderWithUnixTime(std::string_view ledgerHash, ripple::LedgerIndex seq, uint64_t closeTimeUnixStamp);

/*
 * Create a Legacy (pre XRPFees amendment) FeeSetting ledger object
 */
[[nodiscard]] ripple::STObject
createLegacyFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
);

/*
 * Create a FeeSetting ledger object
 */
ripple::STObject
createFeeSettingLedgerObject(
    ripple::STAmount base,
    ripple::STAmount reserveInc,
    ripple::STAmount reserveBase,
    uint32_t flag
);

/*
 * Create a Legacy (pre XRPFees amendment) FeeSetting ledger object and return its blob
 */
[[nodiscard]] ripple::Blob
createLegacyFeeSettingBlob(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
);

/*
 * Create a FeeSetting ledger object and return its blob
 */
ripple::Blob
createFeeSettingBlob(ripple::STAmount base, ripple::STAmount reserveInc, ripple::STAmount reserveBase, uint32_t flag);

/*
 * Create a payment transaction object
 */
[[nodiscard]] ripple::STObject
createPaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq
);

[[nodiscard]] ripple::STObject
createPaymentTransactionMetaObject(
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
createAccountRootObject(
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
createCreateOfferTransactionObject(
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
getIssue(std::string_view currency, std::string_view issuerId);

/*
 * Create a offer change meta data
 */
[[nodiscard]] ripple::STObject
createMetaDataForBookChange(
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
createMetaDataForCreateOffer(
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
createMetaDataForCancelOffer(
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
createOwnerDirLedgerObject(std::vector<ripple::uint256> indexes, std::string_view rootIndex);

/*
 * Create a payment channel ledger object
 */
[[nodiscard]] ripple::STObject
createPaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq
);

[[nodiscard]] ripple::STObject
createRippleStateLedgerObject(
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
createOfferLedgerObject(
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
createTicketLedgerObject(std::string_view account, uint32_t sequence);

[[nodiscard]] ripple::STObject
createEscrowLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] ripple::STObject
createCheckLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] ripple::STObject
createDepositPreauthLedgerObjectByAuth(std::string_view account, std::string_view auth);

[[nodiscard]] ripple::STObject
createDepositPreauthLedgerObjectByAuthCredentials(
    std::string_view account,
    std::string_view issuer,
    std::string_view credType
);

[[nodiscard]] data::NFT
createNft(
    std::string_view tokenID,
    std::string_view account,
    ripple::LedgerIndex seq = 1234u,
    ripple::Blob uri = ripple::Blob{'u', 'r', 'i'},
    bool isBurned = false
);

[[nodiscard]] ripple::STObject
createNftBuyOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] ripple::STObject
createNftSellOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] ripple::STObject
createSignerLists(std::vector<std::pair<std::string, uint32_t>> const& signers);

[[nodiscard]] ripple::STObject
createNftTokenPage(
    std::vector<std::pair<std::string, std::string>> const& tokens,
    std::optional<ripple::uint256> previousPage
);

[[nodiscard]] data::TransactionAndMetadata
createMintNftTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::string_view nftID
);

[[nodiscard]] data::TransactionAndMetadata
createAcceptNftOfferTxWithMetadata(std::string_view accountId, uint32_t seq, uint32_t fee, std::string_view nftId);

[[nodiscard]] data::TransactionAndMetadata
createCancelNftOffersTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::vector<std::string> const& nftOffers
);

[[nodiscard]] data::TransactionAndMetadata
createCreateNftOfferTxWithMetadata(std::string_view accountId, uint32_t seq, uint32_t fee, std::string_view offerId);

[[nodiscard]] data::TransactionAndMetadata
createCreateNftOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::uint32_t offerPrice,
    std::string_view offerId
);

[[nodiscard]] ripple::STObject
createAmendmentsObject(std::vector<ripple::uint256> const& enabledAmendments);

[[nodiscard]] ripple::STObject
createBrokenAmendmentsObject();

[[nodiscard]] ripple::STObject
createAmmObject(
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
createBridgeObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

[[nodiscard]] ripple::STObject
createChainOwnedClaimIdObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer,
    std::string_view otherChainSource
);

[[nodiscard]] ripple::STObject
createChainOwnedCreateAccountClaimId(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

void
ammAddVoteSlot(ripple::STObject& amm, ripple::AccountID const& accountId, uint16_t tradingFee, uint32_t voteWeight);

void
ammSetAuctionSlot(
    ripple::STObject& amm,
    ripple::AccountID const& accountId,
    ripple::STAmount price,
    uint16_t discountedFee,
    uint32_t expiration,
    std::vector<ripple::AccountID> const& authAccounts = {}
);

[[nodiscard]] ripple::STObject
createDidObject(std::string_view accountId, std::string_view didDoc, std::string_view uri, std::string_view data);

[[nodiscard]] ripple::Currency
createLptCurrency(std::string_view assetCurrency, std::string_view asset2Currency);

[[nodiscard]] ripple::STObject
createMptIssuanceObject(std::string_view accountId, std::uint32_t seq, std::string_view metadata);

[[nodiscard]] ripple::STObject
createMpTokenObject(std::string_view accountId, ripple::uint192 issuanceID, std::uint64_t mptAmount = 1);

[[nodiscard]] ripple::STObject
createOraclePriceData(
    uint64_t assetPrice,
    ripple::Currency baseAssetCurrency,
    ripple::Currency quoteAssetCurrency,
    uint8_t scale
);

[[nodiscard]] ripple::STArray
createPriceDataSeries(std::vector<ripple::STObject> const& series);

[[nodiscard]] ripple::STObject
createOracleObject(
    std::string_view accountId,
    std::string_view provider,
    uint64_t ownerNode,
    uint32_t lastUpdateTime,
    ripple::Blob uri,
    ripple::Blob assetClass,
    uint32_t previousTxSeq,
    ripple::uint256 previousTxId,
    ripple::STArray priceDataSeries
);

[[nodiscard]] data::TransactionAndMetadata
createOracleSetTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t docId,
    std::uint32_t lastUpdateTime,
    ripple::STArray priceDataSeries,
    std::string_view oracleIndex,
    bool created,
    std::string_view previousTxnId
);

[[nodiscard]] ripple::STObject
createCredentialObject(
    std::string_view acc1,
    std::string_view acc2,
    std::string_view credType,
    bool accept = true,
    std::optional<uint32_t> expiration = std::nullopt
);

[[nodiscard]] ripple::STArray
createAuthCredentialArray(std::vector<std::string_view> issuer, std::vector<std::string_view> credType);
