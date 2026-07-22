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
[[nodiscard]] xrpl::AccountID
getAccountIdWithString(std::string_view id);

/**
 * Create AccountID object with string and return its key
 */
[[nodiscard]] xrpl::uint256
getAccountKey(std::string_view id);

/*
 * Gets the account key from an account id
 */
[[nodiscard]] xrpl::uint256
getAccountKey(xrpl::AccountID const& acc);

/*
 * Create a simple ledgerHeader object with only hash and seq
 */
[[nodiscard]] xrpl::LedgerHeader
createLedgerHeader(
    std::string_view ledgerHash,
    xrpl::LedgerIndex seq,
    std::optional<uint32_t> age = std::nullopt
);

/*
 * Create a simple ledgerHeader object with hash, seq and unix timestamp
 */
[[nodiscard]] xrpl::LedgerHeader
createLedgerHeaderWithUnixTime(
    std::string_view ledgerHash,
    xrpl::LedgerIndex seq,
    uint64_t closeTimeUnixStamp
);

/*
 * Create a Legacy (pre XRPFees amendment) FeeSetting ledger object
 */
[[nodiscard]] xrpl::STObject
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
xrpl::STObject
createFeeSettingLedgerObject(
    xrpl::STAmount base,
    xrpl::STAmount reserveInc,
    xrpl::STAmount reserveBase,
    uint32_t flag
);

/*
 * Create a Legacy (pre XRPFees amendment) FeeSetting ledger object and return its blob
 */
[[nodiscard]] xrpl::Blob
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
xrpl::Blob
createFeeSettingBlob(
    xrpl::STAmount base,
    xrpl::STAmount reserveInc,
    xrpl::STAmount reserveBase,
    uint32_t flag
);

/*
 * Create a payment transaction object
 */
[[nodiscard]] xrpl::STObject
createPaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq
);

[[nodiscard]] xrpl::STObject
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
[[nodiscard]] xrpl::STObject
createAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq,
    uint32_t transferRate = 0,
    std::optional<xrpl::uint256> ammID = std::nullopt
);

/*
 * Create a createoffer treansaction
 * Taker pay is XRP
 * If reverse is true, taker gets is XRP
 */
[[nodiscard]] xrpl::STObject
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
[[nodiscard]] xrpl::Issue
getIssue(std::string_view currency, std::string_view issuerId);

/*
 * Create a offer change meta data
 */
[[nodiscard]] xrpl::STObject
createMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int previousTakerGets,
    int finalTakerPays,
    int previousTakerPays,
    std::optional<std::string_view> domain = std::nullopt
);

/*
 * Meta data for adding a offer object
 * finalTakerGets is XRP
 * If reverse is true, finalTakerPays is XRP
 */
[[nodiscard]] xrpl::STObject
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
[[nodiscard]] xrpl::STObject
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
[[nodiscard]] xrpl::STObject
createOwnerDirLedgerObject(std::vector<xrpl::uint256> indexes, std::string_view rootIndex);

/*
 * Create a payment channel ledger object
 */
[[nodiscard]] xrpl::STObject
createPaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq
);

[[nodiscard]] xrpl::STObject
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

[[nodiscard]] xrpl::STObject
createOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view paysCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view bookDirId,
    std::optional<std::string_view> domain = std::nullopt
);

[[nodiscard]] xrpl::STObject
createTicketLedgerObject(std::string_view account, uint32_t sequence);

[[nodiscard]] xrpl::STObject
createEscrowLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] xrpl::STObject
createCheckLedgerObject(std::string_view account, std::string_view dest);

[[nodiscard]] xrpl::STObject
createDepositPreauthLedgerObjectByAuth(std::string_view account, std::string_view auth);

[[nodiscard]] xrpl::STObject
createDepositPreauthLedgerObjectByAuthCredentials(
    std::string_view account,
    std::string_view issuer,
    std::string_view credType
);

[[nodiscard]] data::NFT
createNft(
    std::string_view tokenID,
    std::string_view account,
    xrpl::LedgerIndex seq = 1234u,
    xrpl::Blob uri = xrpl::Blob{'u', 'r', 'i'},
    bool isBurned = false
);

[[nodiscard]] xrpl::STObject
createNftBuyOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] xrpl::STObject
createNftSellOffer(std::string_view tokenID, std::string_view account);

[[nodiscard]] xrpl::STObject
createSignerLists(std::vector<std::pair<std::string, uint32_t>> const& signers);

[[nodiscard]] xrpl::STObject
createNftTokenPage(
    std::vector<std::pair<std::string, std::string>> const& tokens,
    std::optional<xrpl::uint256> previousPage
);

/**
 * Create NFToken mint tx, the metadata contained a changed node
 */
[[nodiscard]] data::TransactionAndMetadata
createMintNftTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::string_view nftID
);

/**
 * Create NFToken mint tx, the metadata contained a created node
 */
[[nodiscard]] data::TransactionAndMetadata
createMintNftTxWithMetadataOfCreatedNode(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::optional<std::string_view> nftID,
    std::optional<std::string_view> uri,
    std::optional<std::string_view> pageIndex
);

[[nodiscard]] data::TransactionAndMetadata
createNftModifyTxWithMetadata(std::string_view accountId, std::string_view nftID, xrpl::Blob uri);

/**
 * Create NFToken burn tx, tx causes a nft page node deleted
 */
[[nodiscard]] data::TransactionAndMetadata
createNftBurnTxWithMetadataOfDeletedNode(std::string_view accountId, std::string_view nftID);

/**
 * Create NFToken mint tx, tx causes a nft page node changed
 */
[[nodiscard]] data::TransactionAndMetadata
createNftBurnTxWithMetadataOfModifiedNode(std::string_view accountId, std::string_view nftID);

[[nodiscard]] data::TransactionAndMetadata
createAcceptNftBuyerOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::string_view offerId
);

[[nodiscard]] data::TransactionAndMetadata
createAcceptNftSellerOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::string_view offerId,
    std::string_view pageIndex,
    bool isNewPageCreated
);

[[nodiscard]] data::TransactionAndMetadata
createCancelNftOffersTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::vector<std::string> const& nftOffers
);

[[nodiscard]] data::TransactionAndMetadata
createCreateNftOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::uint32_t offerPrice,
    std::string_view offerId
);

[[nodiscard]] xrpl::STObject
createAmendmentsObject(std::vector<xrpl::uint256> const& enabledAmendments);

[[nodiscard]] xrpl::STObject
createBrokenAmendmentsObject();

[[nodiscard]] xrpl::STObject
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

[[nodiscard]] xrpl::STObject
createBridgeObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

[[nodiscard]] xrpl::STObject
createChainOwnedClaimIdObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer,
    std::string_view otherChainSource
);

[[nodiscard]] xrpl::STObject
createChainOwnedCreateAccountClaimId(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
);

void
ammAddVoteSlot(
    xrpl::STObject& amm,
    xrpl::AccountID const& accountId,
    uint16_t tradingFee,
    uint32_t voteWeight
);

void
ammSetAuctionSlot(
    xrpl::STObject& amm,
    xrpl::AccountID const& accountId,
    xrpl::STAmount price,
    uint16_t discountedFee,
    uint32_t expiration,
    std::vector<xrpl::AccountID> const& authAccounts = {}
);

[[nodiscard]] xrpl::STObject
createDidObject(
    std::string_view accountId,
    std::string_view didDoc,
    std::string_view uri,
    std::string_view data
);

[[nodiscard]] xrpl::Currency
createLptCurrency(std::string_view assetCurrency, std::string_view asset2Currency);

[[nodiscard]] xrpl::STObject
createMptIssuanceObject(
    std::string_view accountId,
    std::uint32_t seq,
    std::optional<std::string_view> metadata = std::nullopt,
    std::uint32_t flags = 0,
    std::uint64_t outstandingAmount = 0,
    std::optional<std::uint16_t> transferFee = std::nullopt,
    std::optional<std::uint8_t> assetScale = std::nullopt,
    std::optional<std::uint64_t> maxAmount = std::nullopt,
    std::optional<std::uint64_t> lockedAmount = std::nullopt,
    std::optional<std::string_view> domainId = std::nullopt,
    std::optional<std::uint32_t> mutableFlags = std::nullopt
);

[[nodiscard]] xrpl::STObject
createMpTokenObject(
    std::string_view accountId,
    xrpl::uint192 issuanceID,
    std::uint64_t mptAmount = 1,
    std::uint32_t flags = 0,
    std::optional<uint64_t> lockedAmount = std::nullopt
);

[[nodiscard]] xrpl::STObject
createMPTIssuanceCreateTx(std::string_view accountId, uint32_t fee, uint32_t seq);

[[nodiscard]] data::TransactionAndMetadata
createMPTIssuanceCreateTxWithMetadata(std::string_view accountId, uint32_t fee, uint32_t seq);

[[nodiscard]]
xrpl::STObject
createMPTokenAuthorizeTx(
    std::string_view accountId,
    xrpl::uint192 const& mptIssuanceID,
    uint32_t fee,
    uint32_t seq,
    std::optional<std::string_view> holder = std::nullopt,
    std::optional<std::uint32_t> flags = std::nullopt
);

[[nodiscard]]
data::TransactionAndMetadata
createMPTokenAuthorizeTxWithMetadata(
    std::string_view accountId,
    xrpl::uint192 const& mptIssuanceID,
    uint32_t fee,
    uint32_t seq
);

[[nodiscard]] xrpl::STObject
createPermissionedDomainObject(
    std::string_view accountId,
    std::string_view ledgerIndex,
    xrpl::LedgerIndex seq,
    uint64_t ownerNode,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
);

[[nodiscard]] xrpl::STObject
createDelegateObject(
    std::string_view accountId,
    std::string_view authorize,
    std::string_view ledgerIndex,
    uint64_t ownerNode,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
);

[[nodiscard]] xrpl::STObject
createOraclePriceData(
    uint64_t assetPrice,
    xrpl::Currency baseAssetCurrency,
    xrpl::Currency quoteAssetCurrency,
    uint8_t scale
);

[[nodiscard]] xrpl::STArray
createPriceDataSeries(std::vector<xrpl::STObject> const& series);

[[nodiscard]] xrpl::STObject
createOracleObject(
    std::string_view accountId,
    std::string_view provider,
    uint64_t ownerNode,
    uint32_t lastUpdateTime,
    xrpl::Blob uri,
    xrpl::Blob assetClass,
    uint32_t previousTxSeq,
    xrpl::uint256 previousTxId,
    xrpl::STArray priceDataSeries
);

[[nodiscard]] data::TransactionAndMetadata
createOracleSetTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t docId,
    std::uint32_t lastUpdateTime,
    xrpl::STArray priceDataSeries,
    std::string_view oracleIndex,
    bool created,
    std::string_view previousTxnId
);

[[nodiscard]] xrpl::STObject
createCredentialObject(
    std::string_view acc1,
    std::string_view acc2,
    std::string_view credType,
    bool accept = true,
    std::optional<uint32_t> expiration = std::nullopt
);

[[nodiscard]] xrpl::STArray
createAuthCredentialArray(
    std::vector<std::string_view> issuer,
    std::vector<std::string_view> credType
);

[[nodiscard]] xrpl::STObject
createVault(
    std::string_view owner,
    std::string_view account,
    xrpl::LedgerIndex seq,
    std::string_view assetCurrency,
    std::string_view assetIssuer,
    xrpl::uint192 shareMPTID,
    uint64_t ownerNode,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
);

[[nodiscard]] xrpl::STObject
createLoanBroker(
    std::string_view owner,
    std::string_view account,
    xrpl::LedgerIndex seq,
    xrpl::uint256 vaultID,
    uint32_t loanSequence,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
);

[[nodiscard]] xrpl::STObject
createLoan(
    std::string_view borrower,
    xrpl::uint256 loanBrokerID,
    uint32_t loanSequence,
    uint32_t startDate,
    uint32_t paymentInterval,
    int64_t periodicPaymentValue,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
);
