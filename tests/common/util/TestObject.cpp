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

#include "util/TestObject.hpp"

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Assert.hpp"

#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STIssue.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STVector256.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
}  // namespace

ripple::AccountID
getAccountIdWithString(std::string_view id)
{
    return util::parseBase58Wrapper<ripple::AccountID>(std::string(id)).value();
}

ripple::uint256
getAccountKey(std::string_view id)
{
    return ripple::keylet::account(getAccountIdWithString(id)).key;
}

ripple::uint256
getAccountKey(ripple::AccountID const& acc)
{
    return ripple::keylet::account(acc).key;
}

ripple::LedgerHeader
createLedgerHeader(std::string_view ledgerHash, ripple::LedgerIndex seq, std::optional<uint32_t> age)
{
    using namespace std::chrono;

    auto ledgerHeader = ripple::LedgerHeader();
    ledgerHeader.hash = ripple::uint256{ledgerHash};
    ledgerHeader.seq = seq;

    if (age) {
        auto const now = duration_cast<seconds>(system_clock::now().time_since_epoch());
        auto const closeTime = (now - seconds{age.value()}).count() - kRIPPLE_EPOCH_START;
        ledgerHeader.closeTime = ripple::NetClock::time_point{seconds{closeTime}};
    }

    return ledgerHeader;
}

ripple::LedgerHeader
createLedgerHeaderWithUnixTime(std::string_view ledgerHash, ripple::LedgerIndex seq, uint64_t closeTimeUnixStamp)
{
    using namespace std::chrono;

    auto ledgerHeader = ripple::LedgerHeader();
    ledgerHeader.hash = ripple::uint256{ledgerHash};
    ledgerHeader.seq = seq;

    auto const closeTime = closeTimeUnixStamp - seconds{kRIPPLE_EPOCH_START}.count();
    ledgerHeader.closeTime = ripple::NetClock::time_point{seconds{closeTime}};

    return ledgerHeader;
}

ripple::STObject
createLegacyFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
)
{
    ripple::STObject obj(ripple::sfFee);
    obj.setFieldU16(ripple::sfLedgerEntryType, ripple::ltFEE_SETTINGS);
    obj.setFieldU64(ripple::sfBaseFee, base);
    obj.setFieldU32(ripple::sfReserveIncrement, reserveInc);
    obj.setFieldU32(ripple::sfReserveBase, reserveBase);
    obj.setFieldU32(ripple::sfReferenceFeeUnits, refFeeUnit);
    obj.setFieldU32(ripple::sfFlags, flag);
    return obj;
}

ripple::STObject
createFeeSettingLedgerObject(
    ripple::STAmount base,
    ripple::STAmount reserveInc,
    ripple::STAmount reserveBase,
    uint32_t flag
)
{
    ripple::STObject obj(ripple::sfFee);
    obj.setFieldU16(ripple::sfLedgerEntryType, ripple::ltFEE_SETTINGS);
    obj.setFieldAmount(ripple::sfBaseFeeDrops, base);
    obj.setFieldAmount(ripple::sfReserveBaseDrops, reserveBase);
    obj.setFieldAmount(ripple::sfReserveIncrementDrops, reserveInc);
    obj.setFieldU32(ripple::sfFlags, flag);
    return obj;
}

ripple::Blob
createLegacyFeeSettingBlob(uint64_t base, uint32_t reserveInc, uint32_t reserveBase, uint32_t refFeeUnit, uint32_t flag)
{
    auto lo = createLegacyFeeSettingLedgerObject(base, reserveInc, reserveBase, refFeeUnit, flag);
    return lo.getSerializer().peekData();
}

ripple::Blob
createFeeSettingBlob(ripple::STAmount base, ripple::STAmount reserveInc, ripple::STAmount reserveBase, uint32_t flag)
{
    auto lo = createFeeSettingLedgerObject(base, reserveInc, reserveBase, flag);
    return lo.getSerializer().peekData();
}

ripple::STObject
createPaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq
)
{
    ripple::STObject obj(ripple::sfTransaction);
    obj.setFieldU16(ripple::sfTransactionType, ripple::ttPAYMENT);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId1));
    obj.setAccountID(ripple::sfAccount, account.value());
    obj.setFieldAmount(ripple::sfAmount, ripple::STAmount(amount, false));
    obj.setFieldAmount(ripple::sfFee, ripple::STAmount(fee, false));
    auto account2 = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId2));
    obj.setAccountID(ripple::sfDestination, account2.value());
    obj.setFieldU32(ripple::sfSequence, seq);
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    obj.setFieldVL(ripple::sfSigningPubKey, slice);
    return obj;
}

ripple::STObject
createPaymentTransactionMetaObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int finalBalance1,
    int finalBalance2,
    uint32_t transactionIndex
)
{
    ripple::STObject finalFields(ripple::sfFinalFields);
    finalFields.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId1));
    finalFields.setFieldAmount(ripple::sfBalance, ripple::STAmount(finalBalance1));

    ripple::STObject finalFields2(ripple::sfFinalFields);
    finalFields2.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId2));
    finalFields2.setFieldAmount(ripple::sfBalance, ripple::STAmount(finalBalance2));

    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{2};
    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    node.emplace_back(std::move(finalFields));
    metaArray.push_back(node);
    ripple::STObject node2(ripple::sfModifiedNode);
    node2.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    node2.emplace_back(std::move(finalFields2));
    metaArray.push_back(node2);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, transactionIndex);
    return metaObj;
}

ripple::STObject
createDidObject(std::string_view accountId, std::string_view didDoc, std::string_view uri, std::string_view data)
{
    ripple::STObject did(ripple::sfLedgerEntry);
    did.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    did.setFieldU16(ripple::sfLedgerEntryType, ripple::ltDID);
    did.setFieldU32(ripple::sfFlags, 0);
    did.setFieldU64(ripple::sfOwnerNode, 0);
    did.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    did.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    ripple::Slice const sliceDoc(didDoc.data(), didDoc.size());
    did.setFieldVL(ripple::sfDIDDocument, sliceDoc);
    ripple::Slice const sliceUri(uri.data(), uri.size());
    did.setFieldVL(ripple::sfURI, sliceUri);
    ripple::Slice const sliceData(data.data(), data.size());
    did.setFieldVL(ripple::sfData, sliceData);
    return did;
}

ripple::STObject
createAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq,
    uint32_t transferRate
)
{
    ripple::STObject accountRoot(ripple::sfAccount);
    accountRoot.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    accountRoot.setFieldU32(ripple::sfFlags, flag);
    accountRoot.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    accountRoot.setFieldU32(ripple::sfSequence, seq);
    accountRoot.setFieldAmount(ripple::sfBalance, ripple::STAmount(balance, false));
    accountRoot.setFieldU32(ripple::sfOwnerCount, ownerCount);
    accountRoot.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{previousTxnID});
    accountRoot.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    accountRoot.setFieldU32(ripple::sfTransferRate, transferRate);
    return accountRoot;
}

ripple::STObject
createCreateOfferTransactionObject(
    std::string_view accountId,
    int fee,
    uint32_t seq,
    std::string_view currency,
    std::string_view issuer,
    int takerGets,
    int takerPays,
    bool reverse
)
{
    ripple::STObject obj(ripple::sfTransaction);
    obj.setFieldU16(ripple::sfTransactionType, ripple::ttOFFER_CREATE);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    obj.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    obj.setFieldAmount(ripple::sfFee, amount);
    obj.setFieldU32(ripple::sfSequence, seq);
    // add amount
    ripple::Issue const issue1(
        ripple::Currency{currency}, util::parseBase58Wrapper<ripple::AccountID>(std::string(issuer)).value()
    );
    if (reverse) {
        obj.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue1, takerGets));
        obj.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(takerPays, false));
    } else {
        obj.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(issue1, takerGets));
        obj.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(takerPays, false));
    }

    auto key = "test";
    ripple::Slice const slice(key, 4);
    obj.setFieldVL(ripple::sfSigningPubKey, slice);
    return obj;
}

ripple::Issue
getIssue(std::string_view currency, std::string_view issuerId)
{
    // standard currency
    if (currency.size() == 3) {
        return ripple::Issue(
            ripple::to_currency(std::string(currency)),
            util::parseBase58Wrapper<ripple::AccountID>(std::string(issuerId)).value()
        );
    }
    return ripple::Issue(
        ripple::Currency{currency}, util::parseBase58Wrapper<ripple::AccountID>(std::string(issuerId)).value()
    );
}

ripple::STObject
createMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int perviousTakerGets,
    int finalTakerPays,
    int perviousTakerPays
)
{
    ripple::STObject finalFields(ripple::sfFinalFields);
    ripple::Issue const issue1 = getIssue(currency, issueId);
    finalFields.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
    ripple::STObject previousFields(ripple::sfPreviousFields);
    previousFields.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue1, perviousTakerPays));
    previousFields.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(perviousTakerGets, false));
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltOFFER);
    node.emplace_back(std::move(finalFields));
    node.emplace_back(std::move(previousFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, transactionIndex);
    return metaObj;
}

ripple::STObject
createMetaDataForCreateOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays,
    bool reverse
)
{
    ripple::STObject finalFields(ripple::sfNewFields);
    ripple::Issue const issue1 = getIssue(currency, issueId);
    if (reverse) {
        finalFields.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(issue1, finalTakerPays));
        finalFields.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(finalTakerGets, false));
    } else {
        finalFields.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
        finalFields.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
    }
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfCreatedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltOFFER);
    node.emplace_back(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, transactionIndex);
    return metaObj;
}

ripple::STObject
createMetaDataForCancelOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays
)
{
    ripple::STObject finalFields(ripple::sfFinalFields);
    ripple::Issue const issue1 = getIssue(currency, issueId);
    finalFields.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfDeletedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltOFFER);
    node.emplace_back(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, transactionIndex);
    return metaObj;
}

ripple::STObject
createOwnerDirLedgerObject(std::vector<ripple::uint256> indexes, std::string_view rootIndex)
{
    ripple::STObject ownerDir(ripple::sfLedgerEntry);
    ownerDir.setFieldU16(ripple::sfLedgerEntryType, ripple::ltDIR_NODE);
    ownerDir.setFieldV256(ripple::sfIndexes, ripple::STVector256{indexes});
    ownerDir.setFieldH256(ripple::sfRootIndex, ripple::uint256{rootIndex});
    ownerDir.setFieldU32(ripple::sfFlags, 0);
    return ownerDir;
}

ripple::STObject
createPaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq
)
{
    ripple::STObject channel(ripple::sfLedgerEntry);
    channel.setFieldU16(ripple::sfLedgerEntryType, ripple::ltPAYCHAN);
    channel.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    channel.setAccountID(ripple::sfDestination, getAccountIdWithString(destId));
    channel.setFieldAmount(ripple::sfAmount, ripple::STAmount(amount, false));
    channel.setFieldAmount(ripple::sfBalance, ripple::STAmount(balance, false));
    channel.setFieldU32(ripple::sfSettleDelay, settleDelay);
    channel.setFieldU64(ripple::sfOwnerNode, 0);
    channel.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{previousTxnId});
    channel.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    channel.setFieldU32(ripple::sfFlags, 0);
    uint8_t key[33] = {0};
    key[0] = 2;  // KeyType::secp256k1
    ripple::Slice const slice(key, 33);
    channel.setFieldVL(ripple::sfPublicKey, slice);
    return channel;
}

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
    uint32_t flag
)
{
    auto line = ripple::STObject(ripple::sfLedgerEntry);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldU32(ripple::sfFlags, flag);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(getIssue(currency, issuerId), balance));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(getIssue(currency, highNodeAccountId), highLimit));
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(getIssue(currency, lowNodeAccountId), lowLimit));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{previousTxnId});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    return line;
}

ripple::STObject
createOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view paysCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view dirId
)
{
    ripple::STObject offer(ripple::sfLedgerEntry);
    offer.setFieldU16(ripple::sfLedgerEntryType, ripple::ltOFFER);
    offer.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    offer.setFieldU32(ripple::sfSequence, 0);
    offer.setFieldU32(ripple::sfFlags, 0);
    ripple::Issue const issue1 = getIssue(getsCurrency, getsIssueId);
    offer.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(issue1, takerGets));
    ripple::Issue const issue2 = getIssue(paysCurrency, paysIssueId);
    offer.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(issue2, takerPays));
    offer.setFieldH256(ripple::sfBookDirectory, ripple::uint256{});
    offer.setFieldU64(ripple::sfBookNode, 0);
    offer.setFieldU64(ripple::sfOwnerNode, 0);
    offer.setFieldH256(ripple::sfBookDirectory, ripple::uint256{dirId});
    offer.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    offer.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return offer;
}

ripple::STObject
createTicketLedgerObject(std::string_view account, uint32_t sequence)
{
    ripple::STObject ticket(ripple::sfLedgerEntry);
    ticket.setFieldU16(ripple::sfLedgerEntryType, ripple::ltTICKET);
    ticket.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    ticket.setFieldU32(ripple::sfFlags, 0);
    ticket.setFieldU64(ripple::sfOwnerNode, 0);
    ticket.setFieldU32(ripple::sfTicketSequence, sequence);
    ticket.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    ticket.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return ticket;
}

ripple::STObject
createEscrowLedgerObject(std::string_view account, std::string_view dest)
{
    ripple::STObject escrow(ripple::sfLedgerEntry);
    escrow.setFieldU16(ripple::sfLedgerEntryType, ripple::ltESCROW);
    escrow.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    escrow.setAccountID(ripple::sfDestination, getAccountIdWithString(dest));
    escrow.setFieldAmount(ripple::sfAmount, ripple::STAmount(0, false));
    escrow.setFieldU64(ripple::sfOwnerNode, 0);
    escrow.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    escrow.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    escrow.setFieldU32(ripple::sfFlags, 0);
    return escrow;
}

ripple::STObject
createCheckLedgerObject(std::string_view account, std::string_view dest)
{
    ripple::STObject check(ripple::sfLedgerEntry);
    check.setFieldU16(ripple::sfLedgerEntryType, ripple::ltCHECK);
    check.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    check.setAccountID(ripple::sfDestination, getAccountIdWithString(dest));
    check.setFieldU32(ripple::sfFlags, 0);
    check.setFieldU64(ripple::sfOwnerNode, 0);
    check.setFieldU64(ripple::sfDestinationNode, 0);
    check.setFieldAmount(ripple::sfSendMax, ripple::STAmount(0, false));
    check.setFieldU32(ripple::sfSequence, 0);
    check.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    check.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return check;
}

ripple::STObject
createDepositPreauthLedgerObjectByAuth(std::string_view account, std::string_view auth)
{
    ripple::STObject depositPreauth(ripple::sfLedgerEntry);
    depositPreauth.setFieldU16(ripple::sfLedgerEntryType, ripple::ltDEPOSIT_PREAUTH);
    depositPreauth.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    depositPreauth.setAccountID(ripple::sfAuthorize, getAccountIdWithString(auth));
    depositPreauth.setFieldU32(ripple::sfFlags, 0);
    depositPreauth.setFieldU64(ripple::sfOwnerNode, 0);
    depositPreauth.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    depositPreauth.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return depositPreauth;
}

ripple::STObject
createDepositPreauthLedgerObjectByAuthCredentials(
    std::string_view account,
    std::string_view issuer,
    std::string_view credType
)
{
    ripple::STObject depositPreauth(ripple::sfLedgerEntry);
    depositPreauth.setFieldU16(ripple::sfLedgerEntryType, ripple::ltDEPOSIT_PREAUTH);
    depositPreauth.setAccountID(ripple::sfAccount, getAccountIdWithString(account));
    depositPreauth.setFieldArray(
        ripple::sfAuthorizeCredentials,
        createAuthCredentialArray(std::vector<std::string_view>{issuer}, std::vector<std::string_view>{credType})
    );
    depositPreauth.setFieldU32(ripple::sfFlags, 0);
    depositPreauth.setFieldU64(ripple::sfOwnerNode, 0);
    depositPreauth.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    depositPreauth.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return depositPreauth;
}

data::NFT
createNft(std::string_view tokenID, std::string_view account, ripple::LedgerIndex seq, ripple::Blob uri, bool isBurned)
{
    return data::NFT{ripple::uint256(tokenID), seq, getAccountIdWithString(account), uri, isBurned};
}

ripple::STObject
createNftBuyOffer(std::string_view tokenID, std::string_view account)
{
    ripple::STObject offer(ripple::sfLedgerEntry);
    offer.setFieldH256(ripple::sfNFTokenID, ripple::uint256{tokenID});
    offer.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_OFFER);
    offer.setFieldU32(ripple::sfFlags, 0u);
    offer.setFieldAmount(ripple::sfAmount, ripple::STAmount{123});
    offer.setFieldU64(ripple::sfOwnerNode, 0ul);
    offer.setAccountID(ripple::sfOwner, getAccountIdWithString(account));
    offer.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    offer.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0u);
    offer.setFieldU64(ripple::sfNFTokenOfferNode, 0ul);
    return offer;
}

ripple::STObject
createNftSellOffer(std::string_view tokenID, std::string_view account)
{
    ripple::STObject offer(ripple::sfLedgerEntry);
    offer.setFieldH256(ripple::sfNFTokenID, ripple::uint256{tokenID});
    offer.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_OFFER);
    offer.setFieldU32(ripple::sfFlags, 0u);
    offer.setFieldAmount(ripple::sfAmount, ripple::STAmount{123});
    offer.setFieldU64(ripple::sfOwnerNode, 0ul);
    offer.setAccountID(ripple::sfOwner, getAccountIdWithString(account));
    offer.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    offer.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0u);
    offer.setFieldU64(ripple::sfNFTokenOfferNode, 0ul);
    return offer;
}

ripple::STObject
createSignerLists(std::vector<std::pair<std::string, uint32_t>> const& signers)
{
    auto signerlists = ripple::STObject(ripple::sfLedgerEntry);
    signerlists.setFieldU16(ripple::sfLedgerEntryType, ripple::ltSIGNER_LIST);
    signerlists.setFieldU32(ripple::sfFlags, 0);
    signerlists.setFieldU64(ripple::sfOwnerNode, 0);
    signerlists.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256());
    signerlists.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    signerlists.setFieldU32(ripple::sfSignerListID, 0);
    uint32_t quorum = 0;
    ripple::STArray list;
    for (auto const& signer : signers) {
        auto entry = ripple::STObject(ripple::sfSignerEntry);
        entry.setAccountID(ripple::sfAccount, getAccountIdWithString(signer.first));
        entry.setFieldU16(ripple::sfSignerWeight, signer.second);
        quorum += signer.second;
        list.push_back(std::move(entry));
    }
    signerlists.setFieldU32(ripple::sfSignerQuorum, quorum);
    signerlists.setFieldArray(ripple::sfSignerEntries, list);
    return signerlists;
}

ripple::STObject
createNftTokenPage(
    std::vector<std::pair<std::string, std::string>> const& tokens,
    std::optional<ripple::uint256> previousPage
)
{
    auto tokenPage = ripple::STObject(ripple::sfLedgerEntry);
    tokenPage.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_PAGE);
    tokenPage.setFieldU32(ripple::sfFlags, 0);
    tokenPage.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256());
    tokenPage.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    if (previousPage)
        tokenPage.setFieldH256(ripple::sfPreviousPageMin, *previousPage);
    ripple::STArray list;
    for (auto const& token : tokens) {
        auto entry = ripple::STObject(ripple::sfNFToken);
        entry.setFieldH256(ripple::sfNFTokenID, ripple::uint256{token.first.c_str()});
        entry.setFieldVL(ripple::sfURI, ripple::Slice(token.second.c_str(), token.second.size()));
        list.push_back(std::move(entry));
    }
    tokenPage.setFieldArray(ripple::sfNFTokens, list);
    return tokenPage;
}

data::TransactionAndMetadata
createMintNftTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::string_view nftID
)
{
    // tx
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttNFTOKEN_MINT);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    tx.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    tx.setFieldAmount(ripple::sfFee, amount);
    // required field for ttNFTOKEN_MINT
    tx.setFieldU32(ripple::sfNFTokenTaxon, nfTokenTaxon);
    tx.setFieldU32(ripple::sfSequence, seq);
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    tx.setFieldVL(ripple::sfSigningPubKey, slice);

    // meta
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_PAGE);

    ripple::STObject finalFields(ripple::sfFinalFields);
    ripple::STArray nftArray1{2};

    // finalFields contain new NFT while previousFields does not
    auto entry = ripple::STObject(ripple::sfNFToken);
    entry.setFieldH256(ripple::sfNFTokenID, ripple::uint256{nftID});
    char const* url = "testurl";
    entry.setFieldVL(ripple::sfURI, ripple::Slice(url, 7));
    nftArray1.push_back(entry);

    auto entry2 = ripple::STObject(ripple::sfNFToken);
    entry2.setFieldH256(ripple::sfNFTokenID, ripple::uint256{kINDEX1});
    entry2.setFieldVL(ripple::sfURI, ripple::Slice(url, 7));
    nftArray1.push_back(entry2);

    finalFields.setFieldArray(ripple::sfNFTokens, nftArray1);

    nftArray1.erase(nftArray1.begin());
    ripple::STObject previousFields(ripple::sfPreviousFields);
    previousFields.setFieldArray(ripple::sfNFTokens, nftArray1);

    node.emplace_back(std::move(finalFields));
    node.emplace_back(std::move(previousFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createAcceptNftOfferTxWithMetadata(std::string_view accountId, uint32_t seq, uint32_t fee, std::string_view nftId)
{
    // tx
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttNFTOKEN_ACCEPT_OFFER);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    tx.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    tx.setFieldAmount(ripple::sfFee, amount);
    tx.setFieldU32(ripple::sfSequence, seq);
    tx.setFieldH256(ripple::sfNFTokenBuyOffer, ripple::uint256{kINDEX1});
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    tx.setFieldVL(ripple::sfSigningPubKey, slice);

    // meta
    // create deletedNode with ltNFTOKEN_OFFER
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfDeletedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_OFFER);

    ripple::STObject finalFields(ripple::sfFinalFields);
    finalFields.setFieldH256(ripple::sfNFTokenID, ripple::uint256{nftId});

    node.emplace_back(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

// NFTokenCancelOffer can be used to cancel multiple offers
data::TransactionAndMetadata
createCancelNftOffersTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::vector<std::string> const& nftOffers
)
{
    // tx
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttNFTOKEN_CANCEL_OFFER);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    tx.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    tx.setFieldAmount(ripple::sfFee, amount);
    tx.setFieldU32(ripple::sfSequence, seq);
    ripple::STVector256 offers;
    offers.resize(nftOffers.size());
    std::ranges::transform(nftOffers, offers.begin(), [&](auto const& nftId) {
        return ripple::uint256{nftId.c_str()};
    });
    tx.setFieldV256(ripple::sfNFTokenOffers, offers);
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    tx.setFieldVL(ripple::sfSigningPubKey, slice);

    // meta
    // create deletedNode with ltNFTOKEN_OFFER
    // reuse the offer id as nft id
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{nftOffers.size()};
    for (auto const& nftId : nftOffers) {
        ripple::STObject node(ripple::sfDeletedNode);
        node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_OFFER);

        ripple::STObject finalFields(ripple::sfFinalFields);
        finalFields.setFieldH256(ripple::sfNFTokenID, ripple::uint256{nftId.c_str()});

        node.emplace_back(std::move(finalFields));
        metaArray.push_back(node);
    }

    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createCreateNftOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::uint32_t offerPrice,
    std::string_view offerId
)
{
    // tx
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttNFTOKEN_CREATE_OFFER);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    tx.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    tx.setFieldAmount(ripple::sfFee, amount);
    auto price = ripple::STAmount(offerPrice, false);
    tx.setFieldAmount(ripple::sfAmount, price);
    tx.setFieldU32(ripple::sfSequence, seq);
    tx.setFieldH256(ripple::sfNFTokenID, ripple::uint256{nftId});
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    tx.setFieldVL(ripple::sfSigningPubKey, slice);

    // meta
    // create createdNode with LedgerIndex
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};

    ripple::STObject node(ripple::sfCreatedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltNFTOKEN_OFFER);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{offerId});

    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
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
)
{
    // tx
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttORACLE_SET);
    auto account = util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId));
    tx.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    tx.setFieldAmount(ripple::sfFee, amount);
    tx.setFieldU32(ripple::sfLastUpdateTime, lastUpdateTime);
    tx.setFieldU32(ripple::sfOracleDocumentID, docId);
    tx.setFieldU32(ripple::sfSequence, seq);
    char const* key = "test";
    ripple::Slice const slice(key, 4);
    tx.setFieldVL(ripple::sfSigningPubKey, slice);
    tx.setFieldArray(ripple::sfPriceDataSeries, priceDataSeries);

    // meta
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    ripple::STArray metaArray{1};

    ripple::STObject node(created ? ripple::sfCreatedNode : ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltORACLE);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{oracleIndex});
    node.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{previousTxnId});
    ripple::STObject fields(created ? ripple::sfNewFields : ripple::sfFinalFields);
    fields.setFieldU32(ripple::sfOracleDocumentID, docId);
    fields.setFieldU32(ripple::sfLastUpdateTime, lastUpdateTime);
    fields.setFieldArray(ripple::sfPriceDataSeries, priceDataSeries);
    node.emplace_back(std::move(fields));

    metaArray.push_back(node);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

ripple::STObject
createAmendmentsObject(std::vector<ripple::uint256> const& enabledAmendments)
{
    auto amendments = ripple::STObject(ripple::sfLedgerEntry);
    amendments.setFieldU16(ripple::sfLedgerEntryType, ripple::ltAMENDMENTS);
    amendments.setFieldU32(ripple::sfFlags, 0);
    ripple::STVector256 const list(enabledAmendments);
    amendments.setFieldV256(ripple::sfAmendments, list);
    return amendments;
}

ripple::STObject
createBrokenAmendmentsObject()
{
    auto amendments = ripple::STObject(ripple::sfLedgerEntry);
    amendments.setFieldU16(ripple::sfLedgerEntryType, ripple::ltAMENDMENTS);
    amendments.setFieldU32(ripple::sfFlags, 0);
    // Note: no sfAmendments present
    return amendments;
}

ripple::STObject
createAmmObject(
    std::string_view accountId,
    std::string_view assetCurrency,
    std::string_view assetIssuer,
    std::string_view asset2Currency,
    std::string_view asset2Issuer,
    std::string_view lpTokenBalanceIssueCurrency,
    uint32_t lpTokenBalanceIssueAmount,
    uint16_t tradingFee,
    uint64_t ownerNode
)
{
    auto amm = ripple::STObject(ripple::sfLedgerEntry);
    amm.setFieldU16(ripple::sfLedgerEntryType, ripple::ltAMM);
    amm.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    amm.setFieldU16(ripple::sfTradingFee, tradingFee);
    amm.setFieldU64(ripple::sfOwnerNode, ownerNode);
    amm.setFieldIssue(ripple::sfAsset, ripple::STIssue{ripple::sfAsset, getIssue(assetCurrency, assetIssuer)});
    amm.setFieldIssue(ripple::sfAsset2, ripple::STIssue{ripple::sfAsset2, getIssue(asset2Currency, asset2Issuer)});
    ripple::Issue const issue1(
        ripple::Currency{lpTokenBalanceIssueCurrency},
        util::parseBase58Wrapper<ripple::AccountID>(std::string(accountId)).value()
    );
    amm.setFieldAmount(ripple::sfLPTokenBalance, ripple::STAmount(issue1, lpTokenBalanceIssueAmount));
    amm.setFieldU32(ripple::sfFlags, 0);
    return amm;
}

ripple::STObject
createBridgeObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
)
{
    auto bridge = ripple::STObject(ripple::sfLedgerEntry);
    bridge.setFieldU16(ripple::sfLedgerEntryType, ripple::ltBRIDGE);
    bridge.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    bridge.setFieldAmount(ripple::sfSignatureReward, ripple::STAmount(10, false));
    bridge.setFieldU64(ripple::sfXChainClaimID, 100);
    bridge.setFieldU64(ripple::sfXChainAccountCreateCount, 100);
    bridge.setFieldU64(ripple::sfXChainAccountClaimCount, 100);
    bridge.setFieldU64(ripple::sfOwnerNode, 100);
    bridge.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    bridge.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    bridge.setFieldU32(ripple::sfFlags, 0);
    Json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    Json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    bridge[ripple::sfXChainBridge] = ripple::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        ripple::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        ripple::issueFromJson(issuingIssue)
    );
    bridge.setFieldU32(ripple::sfFlags, 0);
    return bridge;
}

ripple::STObject
createChainOwnedClaimIdObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer,
    std::string_view otherChainSource
)
{
    auto chainOwnedClaimID = ripple::STObject(ripple::sfLedgerEntry);
    chainOwnedClaimID.setFieldU16(ripple::sfLedgerEntryType, ripple::ltXCHAIN_OWNED_CLAIM_ID);
    chainOwnedClaimID.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    chainOwnedClaimID.setFieldAmount(ripple::sfSignatureReward, ripple::STAmount(10, false));
    chainOwnedClaimID.setFieldU64(ripple::sfXChainClaimID, 100);
    chainOwnedClaimID.setFieldU64(ripple::sfOwnerNode, 100);
    chainOwnedClaimID.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    chainOwnedClaimID.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    chainOwnedClaimID.setFieldU32(ripple::sfFlags, 0);
    Json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    Json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    chainOwnedClaimID[ripple::sfXChainBridge] = ripple::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        ripple::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        ripple::issueFromJson(issuingIssue)
    );
    chainOwnedClaimID.setFieldU32(ripple::sfFlags, 0);
    chainOwnedClaimID.setAccountID(ripple::sfOtherChainSource, getAccountIdWithString(otherChainSource));
    chainOwnedClaimID.setFieldArray(ripple::sfXChainClaimAttestations, ripple::STArray{});
    return chainOwnedClaimID;
}

ripple::STObject
createChainOwnedCreateAccountClaimId(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
)
{
    auto chainOwnedCreateAccountClaimID = ripple::STObject(ripple::sfLedgerEntry);
    chainOwnedCreateAccountClaimID.setFieldU16(ripple::sfLedgerEntryType, ripple::ltXCHAIN_OWNED_CLAIM_ID);
    chainOwnedCreateAccountClaimID.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    chainOwnedCreateAccountClaimID.setFieldU64(ripple::sfXChainAccountCreateCount, 100);
    chainOwnedCreateAccountClaimID.setFieldU64(ripple::sfOwnerNode, 100);
    chainOwnedCreateAccountClaimID.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    chainOwnedCreateAccountClaimID.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    chainOwnedCreateAccountClaimID.setFieldU32(ripple::sfFlags, 0);
    Json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    Json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    chainOwnedCreateAccountClaimID[ripple::sfXChainBridge] = ripple::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        ripple::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        ripple::issueFromJson(issuingIssue)
    );
    chainOwnedCreateAccountClaimID.setFieldU32(ripple::sfFlags, 0);
    chainOwnedCreateAccountClaimID.setFieldArray(ripple::sfXChainCreateAccountAttestations, ripple::STArray{});
    return chainOwnedCreateAccountClaimID;
}

void
ammAddVoteSlot(ripple::STObject& amm, ripple::AccountID const& accountId, uint16_t tradingFee, uint32_t voteWeight)
{
    if (!amm.isFieldPresent(ripple::sfVoteSlots))
        amm.setFieldArray(ripple::sfVoteSlots, ripple::STArray{});

    auto& arr = amm.peekFieldArray(ripple::sfVoteSlots);
    auto slot = ripple::STObject(ripple::sfVoteEntry);
    slot.setAccountID(ripple::sfAccount, accountId);
    slot.setFieldU16(ripple::sfTradingFee, tradingFee);
    slot.setFieldU32(ripple::sfVoteWeight, voteWeight);
    arr.push_back(slot);
}

void
ammSetAuctionSlot(
    ripple::STObject& amm,
    ripple::AccountID const& accountId,
    ripple::STAmount price,
    uint16_t discountedFee,
    uint32_t expiration,
    std::vector<ripple::AccountID> const& authAccounts
)
{
    ASSERT(expiration >= 24 * 3600, "Expiration must be at least 24 hours");

    if (!amm.isFieldPresent(ripple::sfAuctionSlot))
        amm.makeFieldPresent(ripple::sfAuctionSlot);

    auto& auctionSlot = amm.peekFieldObject(ripple::sfAuctionSlot);
    auctionSlot.setAccountID(ripple::sfAccount, accountId);
    auctionSlot.setFieldAmount(ripple::sfPrice, price);
    auctionSlot.setFieldU16(ripple::sfDiscountedFee, discountedFee);
    auctionSlot.setFieldU32(ripple::sfExpiration, expiration);

    if (not authAccounts.empty()) {
        ripple::STArray accounts;

        for (auto const& acc : authAccounts) {
            ripple::STObject authAcc(ripple::sfAuthAccount);
            authAcc.setAccountID(ripple::sfAccount, acc);
            accounts.push_back(authAcc);
        }

        auctionSlot.setFieldArray(ripple::sfAuthAccounts, accounts);
    }
}

ripple::Currency
createLptCurrency(std::string_view assetCurrency, std::string_view asset2Currency)
{
    return ripple::ammLPTCurrency(
        ripple::to_currency(std::string(assetCurrency)), ripple::to_currency(std::string(asset2Currency))
    );
}

ripple::STObject
createMptIssuanceObject(std::string_view accountId, std::uint32_t seq, std::string_view metadata)
{
    ripple::STObject mptIssuance(ripple::sfLedgerEntry);
    mptIssuance.setAccountID(ripple::sfIssuer, getAccountIdWithString(accountId));
    mptIssuance.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN_ISSUANCE);
    mptIssuance.setFieldU32(ripple::sfFlags, 0);
    mptIssuance.setFieldU32(ripple::sfSequence, seq);
    mptIssuance.setFieldU64(ripple::sfOwnerNode, 0);
    mptIssuance.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    mptIssuance.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    mptIssuance.setFieldU64(ripple::sfMaximumAmount, 0);
    mptIssuance.setFieldU64(ripple::sfOutstandingAmount, 0);
    ripple::Slice const sliceMetadata(metadata.data(), metadata.size());
    mptIssuance.setFieldVL(ripple::sfMPTokenMetadata, sliceMetadata);

    return mptIssuance;
}

ripple::STObject
createMpTokenObject(std::string_view accountId, ripple::uint192 issuanceID, std::uint64_t mptAmount)
{
    ripple::STObject mptoken(ripple::sfLedgerEntry);
    mptoken.setAccountID(ripple::sfAccount, getAccountIdWithString(accountId));
    mptoken[ripple::sfMPTokenIssuanceID] = issuanceID;
    mptoken.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN);
    mptoken.setFieldU32(ripple::sfFlags, 0);
    mptoken.setFieldU64(ripple::sfOwnerNode, 0);
    mptoken.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    mptoken.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);

    if (mptAmount != 0u)
        mptoken.setFieldU64(ripple::sfMPTAmount, mptAmount);

    return mptoken;
}

ripple::STObject
createOraclePriceData(
    uint64_t assetPrice,
    ripple::Currency baseAssetCurrency,
    ripple::Currency quoteAssetCurrency,
    uint8_t scale
)
{
    auto priceData = ripple::STObject(ripple::sfPriceData);
    priceData.setFieldU64(ripple::sfAssetPrice, assetPrice);
    priceData.setFieldCurrency(ripple::sfBaseAsset, ripple::STCurrency{ripple::sfBaseAsset, baseAssetCurrency});
    priceData.setFieldCurrency(ripple::sfQuoteAsset, ripple::STCurrency{ripple::sfQuoteAsset, quoteAssetCurrency});
    priceData.setFieldU8(ripple::sfScale, scale);

    return priceData;
}

ripple::STArray
createPriceDataSeries(std::vector<ripple::STObject> const& series)
{
    return ripple::STArray{series.begin(), series.end()};
}

ripple::STObject
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
)
{
    auto ledgerObject = ripple::STObject(ripple::sfLedgerEntry);
    ledgerObject.setFieldU16(ripple::sfLedgerEntryType, ripple::ltORACLE);
    ledgerObject.setFieldU32(ripple::sfFlags, 0);
    ledgerObject.setAccountID(ripple::sfOwner, getAccountIdWithString(accountId));
    ledgerObject.setFieldVL(ripple::sfProvider, ripple::Blob{provider.begin(), provider.end()});
    ledgerObject.setFieldU64(ripple::sfOwnerNode, ownerNode);
    ledgerObject.setFieldU32(ripple::sfLastUpdateTime, lastUpdateTime);
    ledgerObject.setFieldVL(ripple::sfURI, uri);
    ledgerObject.setFieldVL(ripple::sfAssetClass, assetClass);
    ledgerObject.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxSeq);
    ledgerObject.setFieldH256(ripple::sfPreviousTxnID, previousTxId);
    ledgerObject.setFieldArray(ripple::sfPriceDataSeries, priceDataSeries);

    return ledgerObject;
}

// acc2 issue credential for acc1 so acc2 is issuer
ripple::STObject
createCredentialObject(
    std::string_view acc1,
    std::string_view acc2,
    std::string_view credType,
    bool accept,
    std::optional<uint32_t> expiration
)
{
    ripple::STObject credObj(ripple::sfCredential);
    credObj.setFieldU16(ripple::sfLedgerEntryType, ripple::ltCREDENTIAL);
    credObj.setFieldVL(ripple::sfCredentialType, ripple::Blob{credType.begin(), credType.end()});
    credObj.setAccountID(ripple::sfSubject, getAccountIdWithString(acc1));
    credObj.setAccountID(ripple::sfIssuer, getAccountIdWithString(acc2));
    if (expiration.has_value())
        credObj.setFieldU32(ripple::sfExpiration, expiration.value());

    if (accept) {
        credObj.setFieldU32(ripple::sfFlags, ripple::lsfAccepted);
    } else {
        credObj.setFieldU32(ripple::sfFlags, 0);
    }
    credObj.setFieldU64(ripple::sfSubjectNode, 0);
    credObj.setFieldU64(ripple::sfIssuerNode, 0);
    credObj.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    credObj.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return credObj;
}

ripple::STArray
createAuthCredentialArray(std::vector<std::string_view> issuer, std::vector<std::string_view> credType)
{
    ripple::STArray arr;
    ASSERT(issuer.size() == credType.size(), "issuer and credtype vector must be same length");
    for (std::size_t i = 0; i < issuer.size(); ++i) {
        auto credential = ripple::STObject::makeInnerObject(ripple::sfCredential);
        credential.setAccountID(ripple::sfIssuer, getAccountIdWithString(issuer[i]));
        credential.setFieldVL(ripple::sfCredentialType, ripple::strUnHex(std::string(credType[i])).value());
        arr.push_back(credential);
    }
    return arr;
}
