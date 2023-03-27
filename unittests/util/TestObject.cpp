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
    uint32_t previousTxnSeq,
    uint32_t transferRate)
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
    accountRoot.setFieldU32(ripple::sfTransferRate, transferRate);
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
    // standard currency
    if (currency.size() == 3)
        return ripple::Issue(
            ripple::to_currency(std::string(currency)),
            ripple::parseBase58<ripple::AccountID>(std::string(issuerId))
                .value());
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

ripple::STObject
CreateOwnerDirLedgerObject(
    std::vector<ripple::uint256> indexes,
    std::string_view rootIndex)
{
    ripple::STObject ownerDir(ripple::sfLedgerEntry);
    ownerDir.setFieldU16(ripple::sfLedgerEntryType, ripple::ltDIR_NODE);
    ownerDir.setFieldV256(ripple::sfIndexes, ripple::STVector256{indexes});
    ownerDir.setFieldH256(ripple::sfRootIndex, ripple::uint256{rootIndex});
    ownerDir.setFieldU32(ripple::sfFlags, 0);
    return ownerDir;
}

ripple::STObject
CreatePaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq)
{
    ripple::STObject channel(ripple::sfLedgerEntry);
    channel.setFieldU16(ripple::sfLedgerEntryType, ripple::ltPAYCHAN);
    channel.setAccountID(ripple::sfAccount, GetAccountIDWithString(accountId));
    channel.setAccountID(ripple::sfDestination, GetAccountIDWithString(destId));
    channel.setFieldAmount(ripple::sfAmount, ripple::STAmount(amount, false));
    channel.setFieldAmount(ripple::sfBalance, ripple::STAmount(balance, false));
    channel.setFieldU32(ripple::sfSettleDelay, settleDelay);
    channel.setFieldU64(ripple::sfOwnerNode, 0);
    channel.setFieldH256(
        ripple::sfPreviousTxnID, ripple::uint256{previousTxnId});
    channel.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    channel.setFieldU32(ripple::sfFlags, 0);
    uint8_t key[33] = {0};
    key[0] = 2;  // KeyType::secp256k1
    ripple::Slice slice(key, 33);
    channel.setFieldVL(ripple::sfPublicKey, slice);
    return channel;
}

[[nodiscard]] ripple::STObject
CreateRippleStateLedgerObject(
    std::string_view accountId,
    std::string_view currency,
    std::string_view issuerId,
    int balance,
    std::string_view lowNodeAccountId,
    int lowLimit,
    std::string_view highNodeAccountId,
    int highLimit,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq,
    uint32_t flag)
{
    auto line = ripple::STObject(ripple::sfLedgerEntry);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldU32(ripple::sfFlags, flag);
    line.setFieldAmount(
        ripple::sfBalance,
        ripple::STAmount(GetIssue(currency, issuerId), balance));
    line.setFieldAmount(
        ripple::sfHighLimit,
        ripple::STAmount(GetIssue(currency, highNodeAccountId), highLimit));
    line.setFieldAmount(
        ripple::sfLowLimit,
        ripple::STAmount(GetIssue(currency, lowNodeAccountId), lowLimit));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{previousTxnId});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, previousTxnSeq);
    return line;
}

ripple::STObject
CreateOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view paysCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view dirId)
{
    ripple::STObject offer(ripple::sfLedgerEntry);
    offer.setFieldU16(ripple::sfLedgerEntryType, ripple::ltOFFER);
    offer.setAccountID(ripple::sfAccount, GetAccountIDWithString(account));
    offer.setFieldU32(ripple::sfSequence, 0);
    offer.setFieldU32(ripple::sfFlags, 0);
    ripple::Issue issue1 = GetIssue(getsCurrency, getsIssueId);
    offer.setFieldAmount(
        ripple::sfTakerGets, ripple::STAmount(issue1, takerGets));
    ripple::Issue issue2 = GetIssue(paysCurrency, paysIssueId);
    offer.setFieldAmount(
        ripple::sfTakerPays, ripple::STAmount(issue2, takerPays));
    offer.setFieldH256(ripple::sfBookDirectory, ripple::uint256{});
    offer.setFieldU64(ripple::sfBookNode, 0);
    offer.setFieldU64(ripple::sfOwnerNode, 0);
    offer.setFieldH256(ripple::sfBookDirectory, ripple::uint256{dirId});
    offer.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    offer.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return offer;
}

ripple::STObject
CreateTicketLedgerObject(std::string_view account, uint32_t sequence)
{
    ripple::STObject ticket(ripple::sfLedgerEntry);
    ticket.setFieldU16(ripple::sfLedgerEntryType, ripple::ltTICKET);
    ticket.setAccountID(ripple::sfAccount, GetAccountIDWithString(account));
    ticket.setFieldU32(ripple::sfFlags, 0);
    ticket.setFieldU64(ripple::sfOwnerNode, 0);
    ticket.setFieldU32(ripple::sfTicketSequence, sequence);
    ticket.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    ticket.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return ticket;
}

ripple::STObject
CreateEscrowLedgerObject(std::string_view account, std::string_view dest)
{
    ripple::STObject escrow(ripple::sfLedgerEntry);
    escrow.setFieldU16(ripple::sfLedgerEntryType, ripple::ltESCROW);
    escrow.setAccountID(ripple::sfAccount, GetAccountIDWithString(account));
    escrow.setAccountID(ripple::sfDestination, GetAccountIDWithString(dest));
    escrow.setFieldAmount(ripple::sfAmount, ripple::STAmount(0, false));
    escrow.setFieldU64(ripple::sfOwnerNode, 0);
    escrow.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    escrow.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    escrow.setFieldU32(ripple::sfFlags, 0);
    return escrow;
}

ripple::STObject
CreateCheckLedgerObject(std::string_view account, std::string_view dest)
{
    ripple::STObject check(ripple::sfLedgerEntry);
    check.setFieldU16(ripple::sfLedgerEntryType, ripple::ltCHECK);
    check.setAccountID(ripple::sfAccount, GetAccountIDWithString(account));
    check.setAccountID(ripple::sfDestination, GetAccountIDWithString(dest));
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
CreateDepositPreauthLedgerObject(
    std::string_view account,
    std::string_view auth)
{
    ripple::STObject depositPreauth(ripple::sfLedgerEntry);
    depositPreauth.setFieldU16(
        ripple::sfLedgerEntryType, ripple::ltDEPOSIT_PREAUTH);
    depositPreauth.setAccountID(
        ripple::sfAccount, GetAccountIDWithString(account));
    depositPreauth.setAccountID(
        ripple::sfAuthorize, GetAccountIDWithString(auth));
    depositPreauth.setFieldU32(ripple::sfFlags, 0);
    depositPreauth.setFieldU64(ripple::sfOwnerNode, 0);
    depositPreauth.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{});
    depositPreauth.setFieldU32(ripple::sfPreviousTxnLgrSeq, 0);
    return depositPreauth;
}

Backend::NFT
CreateNFT(
    std::string_view tokenID,
    std::string_view account,
    ripple::LedgerIndex seq,
    ripple::Blob uri,
    bool isBurned)
{
    return Backend::NFT{
        ripple::uint256(tokenID),
        seq,
        GetAccountIDWithString(account),
        uri,
        isBurned};
}
