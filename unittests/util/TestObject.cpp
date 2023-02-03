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

#include "TestObject.h"

#include <ripple/protocol/STArray.h>
#include <ripple/protocol/TER.h>

ripple::AccountID
GetAccountIDWithString(std::string_view id)
{
    return ripple::parseBase58<ripple::AccountID>(std::string(id)).value();
}

ripple::LedgerInfo
CreateLedgerInfo(std::string_view ledgerHash, ripple::LedgerIndex seq)
{
    auto ledgerinfo = ripple::LedgerInfo();
    ledgerinfo.hash = ripple::uint256{ledgerHash};
    ledgerinfo.seq = seq;
    return ledgerinfo;
}

ripple::STObject
CreateFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag)
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

ripple::Blob
CreateFeeSettingBlob(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag)
{
    auto lo = CreateFeeSettingLedgerObject(
        base, reserveInc, reserveBase, refFeeUnit, flag);
    return lo.getSerializer().peekData();
}

ripple::STObject
CreatePaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq)
{
    ripple::STObject obj(ripple::sfTransaction);
    obj.setFieldU16(ripple::sfTransactionType, ripple::ttPAYMENT);
    auto account =
        ripple::parseBase58<ripple::AccountID>(std::string(accountId1));
    obj.setAccountID(ripple::sfAccount, account.value());
    obj.setFieldAmount(ripple::sfAmount, ripple::STAmount(amount, false));
    obj.setFieldAmount(ripple::sfFee, ripple::STAmount(fee, false));
    auto account2 =
        ripple::parseBase58<ripple::AccountID>(std::string(accountId2));
    obj.setAccountID(ripple::sfDestination, account2.value());
    obj.setFieldU32(ripple::sfSequence, seq);
    const char* key = "test";
    ripple::Slice slice(key, 4);
    obj.setFieldVL(ripple::sfSigningPubKey, slice);
    return obj;
}

ripple::STObject
CreateAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq)
{
    ripple::STObject accountRoot(ripple::sfAccount);
    accountRoot.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    accountRoot.setFieldU32(ripple::sfFlags, flag);
    accountRoot.setAccountID(
        ripple::sfAccount, GetAccountIDWithString(accountId));
    accountRoot.setFieldU32(ripple::sfSequence, seq);
    accountRoot.setFieldAmount(
        ripple::sfBalance, ripple::STAmount(balance, false));
    accountRoot.setFieldU32(ripple::sfOwnerCount, ownerCount);
    accountRoot.setFieldH256(
        ripple::sfPreviousTxnID, ripple::uint256{previousTxnID});
    accountRoot.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    return accountRoot;
}

ripple::STObject
CreateCreateOfferTransactionObject(
    std::string_view accountId,
    int fee,
    uint32_t seq,
    std::string_view currency,
    std::string_view issuer,
    int takerGets,
    int takerPays)
{
    ripple::STObject obj(ripple::sfTransaction);
    obj.setFieldU16(ripple::sfTransactionType, ripple::ttOFFER_CREATE);
    auto account =
        ripple::parseBase58<ripple::AccountID>(std::string(accountId));
    obj.setAccountID(ripple::sfAccount, account.value());
    auto amount = ripple::STAmount(fee, false);
    obj.setFieldAmount(ripple::sfFee, amount);
    obj.setFieldU32(ripple::sfSequence, seq);
    // add amount
    ripple::Issue issue1(
        ripple::Currency{currency},
        ripple::parseBase58<ripple::AccountID>(std::string(issuer)).value());
    obj.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(issue1, takerGets));
    obj.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(takerPays, false));

    auto key = "test";
    ripple::Slice slice(key, 4);
    obj.setFieldVL(ripple::sfSigningPubKey, slice);
    return obj;
}

ripple::Issue
GetIssue(std::string_view currency, std::string_view issuerId)
{
    return ripple::Issue(
        ripple::Currency{currency},
        ripple::parseBase58<ripple::AccountID>(std::string(issuerId)).value());
}

ripple::STObject
CreateMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int perviousTakerGets,
    int finalTakerPays,
    int perviousTakerPays)
{
    ripple::STObject finalFields(ripple::sfFinalFields);
    ripple::Issue issue1 = GetIssue(currency, issueId);
    finalFields.setFieldAmount(
        ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
    ripple::STObject previousFields(ripple::sfPreviousFields);
    previousFields.setFieldAmount(
        ripple::sfTakerPays, ripple::STAmount(issue1, perviousTakerPays));
    previousFields.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(perviousTakerGets, false));
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
CreateMetaDataForCreateOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays)
{
    ripple::STObject finalFields(ripple::sfNewFields);
    ripple::Issue issue1 = GetIssue(currency, issueId);
    finalFields.setFieldAmount(
        ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
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
CreateMetaDataForCancelOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays)
{
    ripple::STObject finalFields(ripple::sfFinalFields);
    ripple::Issue issue1 = GetIssue(currency, issueId);
    finalFields.setFieldAmount(
        ripple::sfTakerPays, ripple::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(finalTakerGets, false));
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
