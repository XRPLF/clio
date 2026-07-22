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
#include <xrpl/protocol/STNumber.h>
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
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
xrpl::Slice const kSlice("test", 4);
}  // namespace

xrpl::AccountID
getAccountIdWithString(std::string_view id)
{
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return *util::parseBase58Wrapper<xrpl::AccountID>(std::string(id));
}

xrpl::uint256
getAccountKey(std::string_view id)
{
    return xrpl::keylet::account(getAccountIdWithString(id)).key;
}

xrpl::uint256
getAccountKey(xrpl::AccountID const& acc)
{
    return xrpl::keylet::account(acc).key;
}

xrpl::LedgerHeader
createLedgerHeader(std::string_view ledgerHash, xrpl::LedgerIndex seq, std::optional<uint32_t> age)
{
    using namespace std::chrono;

    auto ledgerHeader = xrpl::LedgerHeader();
    ledgerHeader.hash = xrpl::uint256{ledgerHash};
    ledgerHeader.seq = seq;

    if (age) {
        // Note: be cautious of using age values close to each other as the underlying NetClock
        // precision is seconds and the small time difference may lead to comparison bugs
        auto const now = duration_cast<seconds>(system_clock::now().time_since_epoch());
        auto const closeTime = (now - seconds{*age}).count() - kRippleEpochStart;
        ledgerHeader.closeTime = xrpl::NetClock::time_point{seconds{closeTime}};
    }

    return ledgerHeader;
}

xrpl::LedgerHeader
createLedgerHeaderWithUnixTime(
    std::string_view ledgerHash,
    xrpl::LedgerIndex seq,
    uint64_t closeTimeUnixStamp
)
{
    using namespace std::chrono;

    auto ledgerHeader = xrpl::LedgerHeader();
    ledgerHeader.hash = xrpl::uint256{ledgerHash};
    ledgerHeader.seq = seq;

    auto const closeTime = closeTimeUnixStamp - seconds{kRippleEpochStart}.count();
    ledgerHeader.closeTime = xrpl::NetClock::time_point{seconds{closeTime}};

    return ledgerHeader;
}

xrpl::STObject
createLegacyFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
)
{
    xrpl::STObject obj(xrpl::sfFee);
    obj.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltFEE_SETTINGS);
    obj.setFieldU64(xrpl::sfBaseFee, base);
    obj.setFieldU32(xrpl::sfReserveIncrement, reserveInc);
    obj.setFieldU32(xrpl::sfReserveBase, reserveBase);
    obj.setFieldU32(xrpl::sfReferenceFeeUnits, refFeeUnit);
    obj.setFieldU32(xrpl::sfFlags, flag);
    return obj;
}

xrpl::STObject
createFeeSettingLedgerObject(
    xrpl::STAmount base,
    xrpl::STAmount reserveInc,
    xrpl::STAmount reserveBase,
    uint32_t flag
)
{
    xrpl::STObject obj(xrpl::sfFee);
    obj.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltFEE_SETTINGS);
    obj.setFieldAmount(xrpl::sfBaseFeeDrops, base);
    obj.setFieldAmount(xrpl::sfReserveBaseDrops, reserveBase);
    obj.setFieldAmount(xrpl::sfReserveIncrementDrops, reserveInc);
    obj.setFieldU32(xrpl::sfFlags, flag);
    return obj;
}

xrpl::Blob
createLegacyFeeSettingBlob(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag
)
{
    auto lo = createLegacyFeeSettingLedgerObject(base, reserveInc, reserveBase, refFeeUnit, flag);
    return lo.getSerializer().peekData();
}

xrpl::Blob
createFeeSettingBlob(
    xrpl::STAmount base,
    xrpl::STAmount reserveInc,
    xrpl::STAmount reserveBase,
    uint32_t flag
)
{
    auto lo = createFeeSettingLedgerObject(base, reserveInc, reserveBase, flag);
    return lo.getSerializer().peekData();
}

xrpl::STObject
createPaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq
)
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId1));
    obj.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    obj.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(amount, false));
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(fee, false));
    auto account2 = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId2));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    obj.setAccountID(xrpl::sfDestination, *account2);
    obj.setFieldU32(xrpl::sfSequence, seq);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    return obj;
}

xrpl::STObject
createPaymentTransactionMetaObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int finalBalance1,
    int finalBalance2,
    uint32_t transactionIndex
)
{
    xrpl::STObject finalFields(xrpl::sfFinalFields);
    finalFields.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId1));
    finalFields.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(finalBalance1));

    xrpl::STObject finalFields2(xrpl::sfFinalFields);
    finalFields2.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId2));
    finalFields2.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(finalBalance2));

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{2};
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    node.set(std::move(finalFields));
    metaArray.push_back(node);
    xrpl::STObject node2(xrpl::sfModifiedNode);
    node2.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    node2.set(std::move(finalFields2));
    metaArray.push_back(node2);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, transactionIndex);
    return metaObj;
}

xrpl::STObject
createDidObject(
    std::string_view accountId,
    std::string_view didDoc,
    std::string_view uri,
    std::string_view data
)
{
    xrpl::STObject did(xrpl::sfLedgerEntry);
    did.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    did.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltDID);
    did.setFieldU32(xrpl::sfFlags, 0);
    did.setFieldU64(xrpl::sfOwnerNode, 0);
    did.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    did.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    xrpl::Slice const sliceDoc(didDoc.data(), didDoc.size());
    did.setFieldVL(xrpl::sfDIDDocument, sliceDoc);
    xrpl::Slice const sliceUri(uri.data(), uri.size());
    did.setFieldVL(xrpl::sfURI, sliceUri);
    xrpl::Slice const sliceData(data.data(), data.size());
    did.setFieldVL(xrpl::sfData, sliceData);
    return did;
}

xrpl::STObject
createAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq,
    uint32_t transferRate,
    std::optional<xrpl::uint256> ammID
)
{
    xrpl::STObject accountRoot(xrpl::sfAccount);
    accountRoot.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    accountRoot.setFieldU32(xrpl::sfFlags, flag);
    accountRoot.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    accountRoot.setFieldU32(xrpl::sfSequence, seq);
    accountRoot.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(balance, false));
    accountRoot.setFieldU32(xrpl::sfOwnerCount, ownerCount);
    accountRoot.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{previousTxnID});
    accountRoot.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxnSeq);
    accountRoot.setFieldU32(xrpl::sfTransferRate, transferRate);

    if (ammID)
        accountRoot.setFieldH256(xrpl::sfAMMID, *ammID);

    return accountRoot;
}

xrpl::STObject
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
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttOFFER_CREATE);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    obj.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    obj.setFieldAmount(xrpl::sfFee, amount);
    obj.setFieldU32(xrpl::sfSequence, seq);
    // add amount
    xrpl::Issue const issue1(
        xrpl::Currency{currency},
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        *util::parseBase58Wrapper<xrpl::AccountID>(std::string(issuer))
    );
    if (reverse) {
        obj.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue1, takerGets));
        obj.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(takerPays, false));
    } else {
        obj.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(issue1, takerGets));
        obj.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(takerPays, false));
    }

    auto key = "test";
    xrpl::Slice const slice(key, 4);
    obj.setFieldVL(xrpl::sfSigningPubKey, slice);
    return obj;
}

xrpl::Issue
getIssue(std::string_view currency, std::string_view issuerId)
{
    // standard currency
    if (currency.size() == 3) {
        return xrpl::Issue(
            xrpl::toCurrency(std::string(currency)),
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            *util::parseBase58Wrapper<xrpl::AccountID>(std::string(issuerId))
        );
    }
    return xrpl::Issue(
        xrpl::Currency{currency},
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        *util::parseBase58Wrapper<xrpl::AccountID>(std::string(issuerId))
    );
}

xrpl::STObject
createMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int previousTakerGets,
    int finalTakerPays,
    int previousTakerPays,
    std::optional<std::string_view> domain
)
{
    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::Issue const issue1 = getIssue(currency, issueId);
    finalFields.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(finalTakerGets, false));
    if (domain.has_value())
        finalFields.setFieldH256(xrpl::sfDomainID, xrpl::uint256{*domain});
    xrpl::STObject previousFields(xrpl::sfPreviousFields);
    previousFields.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue1, previousTakerPays));
    previousFields.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(previousTakerGets, false));
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    node.set(std::move(finalFields));
    node.set(std::move(previousFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, transactionIndex);
    return metaObj;
}

xrpl::STObject
createMetaDataForCreateOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays,
    bool reverse
)
{
    xrpl::STObject finalFields(xrpl::sfNewFields);
    xrpl::Issue const issue1 = getIssue(currency, issueId);
    if (reverse) {
        finalFields.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(issue1, finalTakerPays));
        finalFields.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(finalTakerGets, false));
    } else {
        finalFields.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue1, finalTakerPays));
        finalFields.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(finalTakerGets, false));
    }
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfCreatedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    node.set(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, transactionIndex);
    return metaObj;
}

xrpl::STObject
createMetaDataForCancelOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays
)
{
    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::Issue const issue1 = getIssue(currency, issueId);
    finalFields.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue1, finalTakerPays));
    finalFields.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(finalTakerGets, false));
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfDeletedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    node.set(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, transactionIndex);
    return metaObj;
}

xrpl::STObject
createOwnerDirLedgerObject(std::vector<xrpl::uint256> indexes, std::string_view rootIndex)
{
    xrpl::STObject ownerDir(xrpl::sfLedgerEntry);
    ownerDir.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltDIR_NODE);
    ownerDir.setFieldV256(xrpl::sfIndexes, xrpl::STVector256{indexes});
    ownerDir.setFieldH256(xrpl::sfRootIndex, xrpl::uint256{rootIndex});
    ownerDir.setFieldU32(xrpl::sfFlags, 0);
    return ownerDir;
}

xrpl::STObject
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
    xrpl::STObject channel(xrpl::sfLedgerEntry);
    channel.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltPAYCHAN);
    channel.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    channel.setAccountID(xrpl::sfDestination, getAccountIdWithString(destId));
    channel.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(amount, false));
    channel.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(balance, false));
    channel.setFieldU32(xrpl::sfSettleDelay, settleDelay);
    channel.setFieldU64(xrpl::sfOwnerNode, 0);
    channel.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{previousTxnId});
    channel.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxnSeq);
    channel.setFieldU32(xrpl::sfFlags, 0);
    uint8_t key[33] = {0};
    key[0] = 2;  // KeyType::secp256k1
    xrpl::Slice const slice(key, 33);
    channel.setFieldVL(xrpl::sfPublicKey, slice);
    return channel;
}

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
    uint32_t flag
)
{
    auto line = xrpl::STObject(xrpl::sfLedgerEntry);
    line.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltRIPPLE_STATE);
    line.setFieldU32(xrpl::sfFlags, flag);
    line.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(getIssue(currency, issuerId), balance));
    line.setFieldAmount(
        xrpl::sfHighLimit, xrpl::STAmount(getIssue(currency, highNodeAccountId), highLimit)
    );
    line.setFieldAmount(
        xrpl::sfLowLimit, xrpl::STAmount(getIssue(currency, lowNodeAccountId), lowLimit)
    );
    line.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{previousTxnId});
    line.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxnSeq);
    return line;
}

xrpl::STObject
createOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view paysCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view dirId,
    std::optional<std::string_view> domain
)
{
    xrpl::STObject offer(xrpl::sfLedgerEntry);
    offer.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    offer.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    offer.setFieldU32(xrpl::sfSequence, 0);
    offer.setFieldU32(xrpl::sfFlags, 0);
    xrpl::Issue const issue1 = getIssue(getsCurrency, getsIssueId);
    offer.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(issue1, takerGets));
    xrpl::Issue const issue2 = getIssue(paysCurrency, paysIssueId);
    offer.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(issue2, takerPays));
    offer.setFieldH256(xrpl::sfBookDirectory, xrpl::uint256{});
    offer.setFieldU64(xrpl::sfBookNode, 0);
    offer.setFieldU64(xrpl::sfOwnerNode, 0);
    offer.setFieldH256(xrpl::sfBookDirectory, xrpl::uint256{dirId});
    offer.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    offer.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    if (domain.has_value())
        offer.setFieldH256(xrpl::sfDomainID, xrpl::uint256{*domain});
    return offer;
}

xrpl::STObject
createTicketLedgerObject(std::string_view account, uint32_t sequence)
{
    xrpl::STObject ticket(xrpl::sfLedgerEntry);
    ticket.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltTICKET);
    ticket.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    ticket.setFieldU32(xrpl::sfFlags, 0);
    ticket.setFieldU64(xrpl::sfOwnerNode, 0);
    ticket.setFieldU32(xrpl::sfTicketSequence, sequence);
    ticket.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    ticket.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    return ticket;
}

xrpl::STObject
createEscrowLedgerObject(std::string_view account, std::string_view dest)
{
    xrpl::STObject escrow(xrpl::sfLedgerEntry);
    escrow.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltESCROW);
    escrow.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    escrow.setAccountID(xrpl::sfDestination, getAccountIdWithString(dest));
    escrow.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(0, false));
    escrow.setFieldU64(xrpl::sfOwnerNode, 0);
    escrow.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    escrow.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    escrow.setFieldU32(xrpl::sfFlags, 0);
    return escrow;
}

xrpl::STObject
createCheckLedgerObject(std::string_view account, std::string_view dest)
{
    xrpl::STObject check(xrpl::sfLedgerEntry);
    check.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltCHECK);
    check.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    check.setAccountID(xrpl::sfDestination, getAccountIdWithString(dest));
    check.setFieldU32(xrpl::sfFlags, 0);
    check.setFieldU64(xrpl::sfOwnerNode, 0);
    check.setFieldU64(xrpl::sfDestinationNode, 0);
    check.setFieldAmount(xrpl::sfSendMax, xrpl::STAmount(0, false));
    check.setFieldU32(xrpl::sfSequence, 0);
    check.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    check.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    return check;
}

xrpl::STObject
createDepositPreauthLedgerObjectByAuth(std::string_view account, std::string_view auth)
{
    xrpl::STObject depositPreauth(xrpl::sfLedgerEntry);
    depositPreauth.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltDEPOSIT_PREAUTH);
    depositPreauth.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    depositPreauth.setAccountID(xrpl::sfAuthorize, getAccountIdWithString(auth));
    depositPreauth.setFieldU32(xrpl::sfFlags, 0);
    depositPreauth.setFieldU64(xrpl::sfOwnerNode, 0);
    depositPreauth.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    depositPreauth.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    return depositPreauth;
}

xrpl::STObject
createDepositPreauthLedgerObjectByAuthCredentials(
    std::string_view account,
    std::string_view issuer,
    std::string_view credType
)
{
    xrpl::STObject depositPreauth(xrpl::sfLedgerEntry);
    depositPreauth.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltDEPOSIT_PREAUTH);
    depositPreauth.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    depositPreauth.setFieldArray(
        xrpl::sfAuthorizeCredentials,
        createAuthCredentialArray(
            std::vector<std::string_view>{issuer}, std::vector<std::string_view>{credType}
        )
    );
    depositPreauth.setFieldU32(xrpl::sfFlags, 0);
    depositPreauth.setFieldU64(xrpl::sfOwnerNode, 0);
    depositPreauth.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    depositPreauth.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    return depositPreauth;
}

data::NFT
createNft(
    std::string_view tokenID,
    std::string_view account,
    xrpl::LedgerIndex seq,
    xrpl::Blob uri,
    bool isBurned
)
{
    return data::NFT{xrpl::uint256(tokenID), seq, getAccountIdWithString(account), uri, isBurned};
}

xrpl::STObject
createNftBuyOffer(std::string_view tokenID, std::string_view account)
{
    xrpl::STObject offer(xrpl::sfLedgerEntry);
    offer.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{tokenID});
    offer.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);
    offer.setFieldU32(xrpl::sfFlags, 0u);
    offer.setFieldAmount(xrpl::sfAmount, xrpl::STAmount{123});
    offer.setFieldU64(xrpl::sfOwnerNode, 0ul);
    offer.setAccountID(xrpl::sfOwner, getAccountIdWithString(account));
    offer.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    offer.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0u);
    offer.setFieldU64(xrpl::sfNFTokenOfferNode, 0ul);
    return offer;
}

xrpl::STObject
createNftSellOffer(std::string_view tokenID, std::string_view account)
{
    xrpl::STObject offer(xrpl::sfLedgerEntry);
    offer.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{tokenID});
    offer.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);
    offer.setFieldU32(xrpl::sfFlags, 0u);
    offer.setFieldAmount(xrpl::sfAmount, xrpl::STAmount{123});
    offer.setFieldU64(xrpl::sfOwnerNode, 0ul);
    offer.setAccountID(xrpl::sfOwner, getAccountIdWithString(account));
    offer.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    offer.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0u);
    offer.setFieldU64(xrpl::sfNFTokenOfferNode, 0ul);
    return offer;
}

xrpl::STObject
createSignerLists(std::vector<std::pair<std::string, uint32_t>> const& signers)
{
    auto signerlists = xrpl::STObject(xrpl::sfLedgerEntry);
    signerlists.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltSIGNER_LIST);
    signerlists.setFieldU32(xrpl::sfFlags, 0);
    signerlists.setFieldU64(xrpl::sfOwnerNode, 0);
    signerlists.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256());
    signerlists.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    signerlists.setFieldU32(xrpl::sfSignerListID, 0);
    uint32_t quorum = 0;
    xrpl::STArray list;
    for (auto const& signer : signers) {
        auto entry = xrpl::STObject(xrpl::sfSignerEntry);
        entry.setAccountID(xrpl::sfAccount, getAccountIdWithString(signer.first));
        entry.setFieldU16(xrpl::sfSignerWeight, signer.second);
        quorum += signer.second;
        list.push_back(std::move(entry));
    }
    signerlists.setFieldU32(xrpl::sfSignerQuorum, quorum);
    signerlists.setFieldArray(xrpl::sfSignerEntries, list);
    return signerlists;
}

xrpl::STObject
createNftTokenPage(
    std::vector<std::pair<std::string, std::string>> const& tokens,
    std::optional<xrpl::uint256> previousPage
)
{
    auto tokenPage = xrpl::STObject(xrpl::sfLedgerEntry);
    tokenPage.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);
    tokenPage.setFieldU32(xrpl::sfFlags, 0);
    tokenPage.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256());
    tokenPage.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    if (previousPage)
        tokenPage.setFieldH256(xrpl::sfPreviousPageMin, *previousPage);
    xrpl::STArray list;
    for (auto const& token : tokens) {
        auto entry = xrpl::STObject(xrpl::sfNFToken);
        entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{token.first.c_str()});
        entry.setFieldVL(xrpl::sfURI, xrpl::Slice(token.second.c_str(), token.second.size()));
        list.push_back(std::move(entry));
    }
    tokenPage.setFieldArray(xrpl::sfNFTokens, list);
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
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_MINT);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    // required field for ttNFTOKEN_MINT
    tx.setFieldU32(xrpl::sfNFTokenTaxon, nfTokenTaxon);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::STArray nftArray1{2};

    // finalFields contain new NFT while previousFields does not
    auto entry = xrpl::STObject(xrpl::sfNFToken);
    entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    char const* url = "testurl";
    entry.setFieldVL(xrpl::sfURI, xrpl::Slice(url, 7));
    nftArray1.push_back(entry);

    auto entry2 = xrpl::STObject(xrpl::sfNFToken);
    entry2.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{kIndex1});
    entry2.setFieldVL(xrpl::sfURI, xrpl::Slice(url, 7));
    nftArray1.push_back(entry2);

    finalFields.setFieldArray(xrpl::sfNFTokens, nftArray1);

    nftArray1.erase(nftArray1.begin());
    xrpl::STObject previousFields(xrpl::sfPreviousFields);
    previousFields.setFieldArray(xrpl::sfNFTokens, nftArray1);

    node.set(std::move(finalFields));
    node.set(std::move(previousFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createMintNftTxWithMetadataOfCreatedNode(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    uint32_t nfTokenTaxon,
    std::optional<std::string_view> nftID,
    std::optional<std::string_view> uri,
    std::optional<std::string_view> pageIndex
)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_MINT);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    // required field for ttNFTOKEN_MINT
    tx.setFieldU32(xrpl::sfNFTokenTaxon, nfTokenTaxon);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    if (uri)
        tx.setFieldVL(xrpl::sfURI, xrpl::Slice(uri->data(), uri->size()));

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfCreatedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

    xrpl::STObject newFields(xrpl::sfNewFields);
    xrpl::STArray nftArray1{1};

    if (nftID) {
        // finalFields contain new NFT while previousFields does not
        auto entry = xrpl::STObject(xrpl::sfNFToken);
        entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{*nftID});
        if (uri)
            entry.setFieldVL(xrpl::sfURI, xrpl::Slice(uri->data(), uri->size()));

        nftArray1.push_back(entry);
    }
    newFields.setFieldArray(xrpl::sfNFTokens, nftArray1);
    node.set(std::move(newFields));
    if (pageIndex)
        node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{*pageIndex});

    // add a ledger object ahead of nft page
    xrpl::STObject node2(xrpl::sfCreatedNode);
    node2.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    metaArray.push_back(node2);

    metaArray.push_back(node);

    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createNftModifyTxWithMetadata(std::string_view accountId, std::string_view nftID, xrpl::Blob uri)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_MODIFY);
    auto account = xrpl::parseBase58<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(10, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    tx.setFieldU32(xrpl::sfSequence, 100);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    if (!uri.empty())  // sfURI should be absent if empty
        tx.setFieldVL(xrpl::sfURI, uri);

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::STArray nftArray1{1};
    xrpl::STArray nftArray2{1};

    // finalFields contain new NFT while previousFields does not
    auto entry = xrpl::STObject(xrpl::sfNFToken);
    entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    if (!uri.empty())
        entry.setFieldVL(xrpl::sfURI, uri);
    nftArray1.push_back(entry);

    auto entry2 = xrpl::STObject(xrpl::sfNFToken);
    entry2.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    char const* url = "previous";
    entry2.setFieldVL(xrpl::sfURI, xrpl::Slice(url, 7));
    nftArray2.push_back(entry2);

    finalFields.setFieldArray(xrpl::sfNFTokens, nftArray1);

    xrpl::STObject previousFields(xrpl::sfPreviousFields);
    previousFields.setFieldArray(xrpl::sfNFTokens, nftArray2);

    node.set(std::move(finalFields));
    node.set(std::move(previousFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createNftBurnTxWithMetadataOfDeletedNode(std::string_view accountId, std::string_view nftID)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_BURN);
    auto account = getAccountIdWithString(accountId);
    tx.setAccountID(xrpl::sfAccount, account);
    auto amount = xrpl::STAmount(10, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    tx.setFieldU32(xrpl::sfSequence, 100);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfDeletedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);
    // deleted node should contain finalFields
    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::STArray nftArray{1};
    auto entry = xrpl::STObject(xrpl::sfNFToken);
    entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    nftArray.push_back(entry);
    finalFields.setFieldArray(xrpl::sfNFTokens, nftArray);

    node.set(std::move(finalFields));

    // add a ledger object ahead of nft page
    xrpl::STObject node2(xrpl::sfCreatedNode);
    node2.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    metaArray.push_back(node2);

    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createNftBurnTxWithMetadataOfModifiedNode(std::string_view accountId, std::string_view nftID)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_BURN);
    auto account = getAccountIdWithString(accountId);
    tx.setAccountID(xrpl::sfAccount, account);
    auto amount = xrpl::STAmount(10, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    tx.setFieldU32(xrpl::sfSequence, 100);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    xrpl::STArray nftArray{1};
    xrpl::STObject previousFields(xrpl::sfPreviousFields);
    auto entry = xrpl::STObject(xrpl::sfNFToken);
    entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftID});
    nftArray.push_back(entry);
    previousFields.setFieldArray(xrpl::sfNFTokens, nftArray);

    node.set(std::move(previousFields));
    node.set(std::move(finalFields));
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createAcceptNftBuyerOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::string_view offerId
)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_ACCEPT_OFFER);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldH256(xrpl::sfNFTokenBuyOffer, xrpl::uint256{offerId});
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    // create deletedNode with ltNFTOKEN_OFFER
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfDeletedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    finalFields.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId});
    // for buyer offer, the offer owner is the nft's new owner
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    finalFields.setAccountID(xrpl::sfOwner, *account);

    node.set(std::move(finalFields));
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{offerId});
    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

data::TransactionAndMetadata
createAcceptNftSellerOfferTxWithMetadata(
    std::string_view accountId,
    uint32_t seq,
    uint32_t fee,
    std::string_view nftId,
    std::string_view offerId,
    std::string_view pageIndex,
    bool isNewPageCreated
)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_ACCEPT_OFFER);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldH256(xrpl::sfNFTokenSellOffer, xrpl::uint256{offerId});
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    // create deletedNode with ltNFTOKEN_OFFER
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};
    xrpl::STObject node(xrpl::sfDeletedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    finalFields.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId});
    // offer owner is not the nft's new owner for seller offer, we need to create other nodes for
    // processing new owner
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    finalFields.setAccountID(xrpl::sfOwner, *account);

    node.set(xrpl::STObject{finalFields});
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{offerId});
    metaArray.push_back(node);

    // new owner's nft page node changed: 1 new nft page node added 2 old nft page node modified
    if (isNewPageCreated) {
        xrpl::STObject node2(xrpl::sfCreatedNode);
        node2.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

        xrpl::STObject newFields(xrpl::sfNewFields);
        xrpl::STArray nftArray1{1};

        auto entry = xrpl::STObject(xrpl::sfNFToken);
        entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId});
        nftArray1.push_back(entry);

        newFields.setFieldArray(xrpl::sfNFTokens, nftArray1);
        node2.set(std::move(newFields));
        node2.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{pageIndex});
        metaArray.push_back(node2);
    } else {
        xrpl::STObject node2(xrpl::sfModifiedNode);
        node2.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_PAGE);

        xrpl::STArray nftArray1{2};

        // finalFields contain new NFT while previousFields does not
        auto entry = xrpl::STObject(xrpl::sfNFToken);
        entry.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId});
        nftArray1.push_back(entry);

        auto entry2 = xrpl::STObject(xrpl::sfNFToken);
        entry2.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{kIndex1});
        nftArray1.push_back(entry2);

        finalFields.setFieldArray(xrpl::sfNFTokens, nftArray1);

        nftArray1.erase(nftArray1.begin());
        xrpl::STObject previousFields(xrpl::sfPreviousFields);
        previousFields.setFieldArray(xrpl::sfNFTokens, nftArray1);

        node2.set(std::move(finalFields));
        node2.set(std::move(previousFields));
        node2.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{pageIndex});
        metaArray.push_back(node2);
    }

    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

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
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_CANCEL_OFFER);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldU32(xrpl::sfSequence, seq);
    xrpl::STVector256 offers;
    offers.resize(nftOffers.size());
    std::ranges::transform(nftOffers, offers.begin(), [&](auto const& nftId) {
        return xrpl::uint256{nftId.c_str()};
    });
    tx.setFieldV256(xrpl::sfNFTokenOffers, offers);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    // create deletedNode with ltNFTOKEN_OFFER
    // reuse the offer id as nft id
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{nftOffers.size()};
    for (auto const& nftId : nftOffers) {
        xrpl::STObject node(xrpl::sfDeletedNode);
        node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);

        xrpl::STObject finalFields(xrpl::sfFinalFields);
        finalFields.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId.c_str()});

        node.set(std::move(finalFields));
        metaArray.push_back(node);
    }

    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

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
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttNFTOKEN_CREATE_OFFER);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    auto price = xrpl::STAmount(offerPrice, false);
    tx.setFieldAmount(xrpl::sfAmount, price);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldH256(xrpl::sfNFTokenID, xrpl::uint256{nftId});
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // meta
    // create createdNode with LedgerIndex
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};

    xrpl::STObject node(xrpl::sfCreatedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltNFTOKEN_OFFER);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{offerId});

    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

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
    xrpl::STArray priceDataSeries,
    std::string_view oracleIndex,
    bool created,
    std::string_view previousTxnId
)
{
    // tx
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttORACLE_SET);
    auto account = util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId));
    tx.setAccountID(xrpl::sfAccount, *account);  // NOLINT(bugprone-unchecked-optional-access)
    auto amount = xrpl::STAmount(fee, false);
    tx.setFieldAmount(xrpl::sfFee, amount);
    tx.setFieldU32(xrpl::sfLastUpdateTime, lastUpdateTime);
    tx.setFieldU32(xrpl::sfOracleDocumentID, docId);
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    tx.setFieldArray(xrpl::sfPriceDataSeries, priceDataSeries);

    // meta
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    xrpl::STArray metaArray{1};

    xrpl::STObject node(created ? xrpl::sfCreatedNode : xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltORACLE);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{oracleIndex});
    node.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{previousTxnId});
    xrpl::STObject fields(created ? xrpl::sfNewFields : xrpl::sfFinalFields);
    fields.setFieldU32(xrpl::sfOracleDocumentID, docId);
    fields.setFieldU32(xrpl::sfLastUpdateTime, lastUpdateTime);
    fields.setFieldArray(xrpl::sfPriceDataSeries, priceDataSeries);
    node.set(std::move(fields));

    metaArray.push_back(node);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

xrpl::STObject
createAmendmentsObject(std::vector<xrpl::uint256> const& enabledAmendments)
{
    auto amendments = xrpl::STObject(xrpl::sfLedgerEntry);
    amendments.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltAMENDMENTS);
    amendments.setFieldU32(xrpl::sfFlags, 0);
    xrpl::STVector256 const list(enabledAmendments);
    amendments.setFieldV256(xrpl::sfAmendments, list);
    return amendments;
}

xrpl::STObject
createBrokenAmendmentsObject()
{
    auto amendments = xrpl::STObject(xrpl::sfLedgerEntry);
    amendments.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltAMENDMENTS);
    amendments.setFieldU32(xrpl::sfFlags, 0);
    // Note: no sfAmendments present
    return amendments;
}

xrpl::STObject
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
    auto amm = xrpl::STObject(xrpl::sfLedgerEntry);
    amm.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltAMM);
    amm.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    amm.setFieldU16(xrpl::sfTradingFee, tradingFee);
    amm.setFieldU64(xrpl::sfOwnerNode, ownerNode);
    amm.setFieldIssue(
        xrpl::sfAsset, xrpl::STIssue{xrpl::sfAsset, getIssue(assetCurrency, assetIssuer)}
    );
    amm.setFieldIssue(
        xrpl::sfAsset2, xrpl::STIssue{xrpl::sfAsset2, getIssue(asset2Currency, asset2Issuer)}
    );
    xrpl::Issue const issue1(
        xrpl::Currency{lpTokenBalanceIssueCurrency},
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        *util::parseBase58Wrapper<xrpl::AccountID>(std::string(accountId))
    );
    amm.setFieldAmount(xrpl::sfLPTokenBalance, xrpl::STAmount(issue1, lpTokenBalanceIssueAmount));
    amm.setFieldU32(xrpl::sfFlags, 0);
    return amm;
}

xrpl::STObject
createBridgeObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
)
{
    auto bridge = xrpl::STObject(xrpl::sfLedgerEntry);
    bridge.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltBRIDGE);
    bridge.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    bridge.setFieldAmount(xrpl::sfSignatureReward, xrpl::STAmount(10, false));
    bridge.setFieldU64(xrpl::sfXChainClaimID, 100);
    bridge.setFieldU64(xrpl::sfXChainAccountCreateCount, 100);
    bridge.setFieldU64(xrpl::sfXChainAccountClaimCount, 100);
    bridge.setFieldU64(xrpl::sfOwnerNode, 100);
    bridge.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    bridge.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    bridge.setFieldU32(xrpl::sfFlags, 0);
    json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    bridge[xrpl::sfXChainBridge] = xrpl::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        xrpl::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        xrpl::issueFromJson(issuingIssue)
    );
    bridge.setFieldU32(xrpl::sfFlags, 0);
    return bridge;
}

xrpl::STObject
createChainOwnedClaimIdObject(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer,
    std::string_view otherChainSource
)
{
    auto chainOwnedClaimID = xrpl::STObject(xrpl::sfLedgerEntry);
    chainOwnedClaimID.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltXCHAIN_OWNED_CLAIM_ID);
    chainOwnedClaimID.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    chainOwnedClaimID.setFieldAmount(xrpl::sfSignatureReward, xrpl::STAmount(10, false));
    chainOwnedClaimID.setFieldU64(xrpl::sfXChainClaimID, 100);
    chainOwnedClaimID.setFieldU64(xrpl::sfOwnerNode, 100);
    chainOwnedClaimID.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    chainOwnedClaimID.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    chainOwnedClaimID.setFieldU32(xrpl::sfFlags, 0);
    json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    chainOwnedClaimID[xrpl::sfXChainBridge] = xrpl::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        xrpl::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        xrpl::issueFromJson(issuingIssue)
    );
    chainOwnedClaimID.setFieldU32(xrpl::sfFlags, 0);
    chainOwnedClaimID.setAccountID(
        xrpl::sfOtherChainSource, getAccountIdWithString(otherChainSource)
    );
    chainOwnedClaimID.setFieldArray(xrpl::sfXChainClaimAttestations, xrpl::STArray{});
    return chainOwnedClaimID;
}

xrpl::STObject
createChainOwnedCreateAccountClaimId(
    std::string_view accountId,
    std::string_view lockingDoor,
    std::string_view issuingDoor,
    std::string_view issuingCurrency,
    std::string_view issuingIssuer
)
{
    auto chainOwnedCreateAccountClaimID = xrpl::STObject(xrpl::sfLedgerEntry);
    chainOwnedCreateAccountClaimID.setFieldU16(
        xrpl::sfLedgerEntryType, xrpl::ltXCHAIN_OWNED_CLAIM_ID
    );
    chainOwnedCreateAccountClaimID.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    chainOwnedCreateAccountClaimID.setFieldU64(xrpl::sfXChainAccountCreateCount, 100);
    chainOwnedCreateAccountClaimID.setFieldU64(xrpl::sfOwnerNode, 100);
    chainOwnedCreateAccountClaimID.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    chainOwnedCreateAccountClaimID.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    chainOwnedCreateAccountClaimID.setFieldU32(xrpl::sfFlags, 0);
    json::Value lockingIssue;
    lockingIssue["currency"] = "XRP";
    json::Value issuingIssue;
    issuingIssue["currency"] = std::string(issuingCurrency);
    issuingIssue["issuer"] = std::string(issuingIssuer);

    chainOwnedCreateAccountClaimID[xrpl::sfXChainBridge] = xrpl::STXChainBridge(
        getAccountIdWithString(lockingDoor),
        xrpl::issueFromJson(lockingIssue),
        getAccountIdWithString(issuingDoor),
        xrpl::issueFromJson(issuingIssue)
    );
    chainOwnedCreateAccountClaimID.setFieldU32(xrpl::sfFlags, 0);
    chainOwnedCreateAccountClaimID.setFieldArray(
        xrpl::sfXChainCreateAccountAttestations, xrpl::STArray{}
    );
    return chainOwnedCreateAccountClaimID;
}

void
ammAddVoteSlot(
    xrpl::STObject& amm,
    xrpl::AccountID const& accountId,
    uint16_t tradingFee,
    uint32_t voteWeight
)
{
    if (!amm.isFieldPresent(xrpl::sfVoteSlots))
        amm.setFieldArray(xrpl::sfVoteSlots, xrpl::STArray{});

    auto& arr = amm.peekFieldArray(xrpl::sfVoteSlots);
    auto slot = xrpl::STObject(xrpl::sfVoteEntry);
    slot.setAccountID(xrpl::sfAccount, accountId);
    slot.setFieldU16(xrpl::sfTradingFee, tradingFee);
    slot.setFieldU32(xrpl::sfVoteWeight, voteWeight);
    arr.push_back(slot);
}

void
ammSetAuctionSlot(
    xrpl::STObject& amm,
    xrpl::AccountID const& accountId,
    xrpl::STAmount price,
    uint16_t discountedFee,
    uint32_t expiration,
    std::vector<xrpl::AccountID> const& authAccounts
)
{
    ASSERT(expiration >= 24 * 3600, "Expiration must be at least 24 hours");

    if (!amm.isFieldPresent(xrpl::sfAuctionSlot))
        amm.makeFieldPresent(xrpl::sfAuctionSlot);

    auto& auctionSlot = amm.peekFieldObject(xrpl::sfAuctionSlot);
    auctionSlot.setAccountID(xrpl::sfAccount, accountId);
    auctionSlot.setFieldAmount(xrpl::sfPrice, price);
    auctionSlot.setFieldU16(xrpl::sfDiscountedFee, discountedFee);
    auctionSlot.setFieldU32(xrpl::sfExpiration, expiration);

    if (not authAccounts.empty()) {
        xrpl::STArray accounts;

        for (auto const& acc : authAccounts) {
            xrpl::STObject authAcc(xrpl::sfAuthAccount);
            authAcc.setAccountID(xrpl::sfAccount, acc);
            accounts.push_back(authAcc);
        }

        auctionSlot.setFieldArray(xrpl::sfAuthAccounts, accounts);
    }
}

xrpl::Currency
createLptCurrency(std::string_view assetCurrency, std::string_view asset2Currency)
{
    return xrpl::ammLPTCurrency(
        xrpl::Issue{xrpl::toCurrency(std::string(assetCurrency)), xrpl::xrpAccount()},
        xrpl::Issue{xrpl::toCurrency(std::string(asset2Currency)), xrpl::xrpAccount()}
    );
}

xrpl::STObject
createMptIssuanceObject(
    std::string_view accountId,
    std::uint32_t seq,
    std::optional<std::string_view> metadata,
    std::uint32_t flags,
    std::uint64_t outstandingAmount,
    std::optional<std::uint16_t> transferFee,
    std::optional<std::uint8_t> assetScale,
    std::optional<std::uint64_t> maxAmount,
    std::optional<std::uint64_t> lockedAmount,
    std::optional<std::string_view> domainId,
    std::optional<std::uint32_t> mutableFlags
)
{
    xrpl::STObject mptIssuance(xrpl::sfLedgerEntry);
    mptIssuance.setAccountID(xrpl::sfIssuer, getAccountIdWithString(accountId));
    mptIssuance.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    mptIssuance.setFieldU32(xrpl::sfSequence, seq);
    mptIssuance.setFieldU64(xrpl::sfOwnerNode, 0);
    mptIssuance.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    mptIssuance.setFieldU32(xrpl::sfFlags, flags);
    mptIssuance.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    mptIssuance.setFieldU64(xrpl::sfOutstandingAmount, outstandingAmount);

    if (transferFee.has_value())
        mptIssuance.setFieldU16(xrpl::sfTransferFee, *transferFee);
    if (assetScale.has_value())
        mptIssuance.setFieldU8(xrpl::sfAssetScale, *assetScale);
    if (maxAmount.has_value())
        mptIssuance.setFieldU64(xrpl::sfMaximumAmount, *maxAmount);
    if (lockedAmount.has_value())
        mptIssuance.setFieldU64(xrpl::sfLockedAmount, *lockedAmount);
    if (metadata.has_value()) {
        xrpl::Slice const sliceMetadata(metadata->data(), metadata->size());
        mptIssuance.setFieldVL(xrpl::sfMPTokenMetadata, sliceMetadata);
    }
    if (domainId.has_value())
        mptIssuance.setFieldH256(xrpl::sfDomainID, xrpl::uint256{*domainId});
    if (mutableFlags.has_value())
        mptIssuance.setFieldU32(xrpl::sfMutableFlags, *mutableFlags);

    return mptIssuance;
}

xrpl::STObject
createMpTokenObject(
    std::string_view accountId,
    xrpl::uint192 issuanceID,
    std::uint64_t mptAmount,
    std::uint32_t flags,
    std::optional<uint64_t> lockedAmount
)
{
    xrpl::STObject mptoken(xrpl::sfLedgerEntry);
    mptoken.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    mptoken[xrpl::sfMPTokenIssuanceID] = issuanceID;
    mptoken.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    mptoken.setFieldU32(xrpl::sfFlags, flags);
    mptoken.setFieldU64(xrpl::sfOwnerNode, 0);
    mptoken.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    mptoken.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);

    if (mptAmount != 0u)
        mptoken.setFieldU64(xrpl::sfMPTAmount, mptAmount);
    if (lockedAmount.has_value())
        mptoken.setFieldU64(xrpl::sfLockedAmount, *lockedAmount);

    return mptoken;
}

xrpl::STObject
createMPTIssuanceCreateTx(std::string_view accountId, uint32_t fee, uint32_t seq)
{
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttMPTOKEN_ISSUANCE_CREATE);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(fee, false));
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    return tx;
}

data::TransactionAndMetadata
createMPTIssuanceCreateTxWithMetadata(std::string_view accountId, uint32_t fee, uint32_t seq)
{
    xrpl::STObject const tx = createMPTIssuanceCreateTx(accountId, fee, seq);

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    xrpl::STObject newFields(xrpl::sfNewFields);
    newFields.setAccountID(xrpl::sfIssuer, getAccountIdWithString(accountId));
    newFields.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    newFields.setFieldU32(xrpl::sfFlags, 0);
    newFields.setFieldU32(xrpl::sfSequence, seq);
    newFields.setFieldU64(xrpl::sfOwnerNode, 0);
    newFields.setFieldU64(xrpl::sfMaximumAmount, 0);
    newFields.setFieldU64(xrpl::sfOutstandingAmount, 0);
    newFields.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    newFields.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    std::string_view const metadata = "test-meta";
    xrpl::Slice const sliceMetadata(metadata.data(), metadata.size());
    newFields.setFieldVL(xrpl::sfMPTokenMetadata, sliceMetadata);

    xrpl::STObject createdNode(xrpl::sfCreatedNode);
    createdNode.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    createdNode.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    createdNode.set(std::move(newFields));

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(std::move(createdNode));
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

xrpl::STObject
createMPTokenAuthorizeTx(
    std::string_view accountId,
    xrpl::uint192 const& mptIssuanceID,
    uint32_t fee,
    uint32_t seq,
    std::optional<std::string_view> holder,
    std::optional<std::uint32_t> flags
)
{
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttMPTOKEN_AUTHORIZE);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    tx[xrpl::sfMPTokenIssuanceID] = mptIssuanceID;
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(fee, false));
    tx.setFieldU32(xrpl::sfSequence, seq);
    tx.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    if (holder)
        tx.setAccountID(xrpl::sfHolder, getAccountIdWithString(*holder));
    if (flags)
        tx.setFieldU32(xrpl::sfFlags, *flags);

    return tx;
}

data::TransactionAndMetadata
createMPTokenAuthorizeTxWithMetadata(
    std::string_view accountId,
    xrpl::uint192 const& mptIssuanceID,
    uint32_t fee,
    uint32_t seq
)
{
    xrpl::STObject const tx = createMPTokenAuthorizeTx(accountId, mptIssuanceID, fee, seq);

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    xrpl::STObject finalFields(xrpl::sfFinalFields);
    finalFields.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    finalFields[xrpl::sfMPTokenIssuanceID] = mptIssuanceID;
    finalFields.setFieldU64(xrpl::sfMPTAmount, 0);

    xrpl::STObject modifiedNode(xrpl::sfModifiedNode);
    modifiedNode.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    modifiedNode.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    modifiedNode.set(std::move(finalFields));

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(std::move(modifiedNode));
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    data::TransactionAndMetadata ret;
    ret.transaction = tx.getSerializer().peekData();
    ret.metadata = metaObj.getSerializer().peekData();
    return ret;
}

xrpl::STObject
createPermissionedDomainObject(
    std::string_view accountId,
    std::string_view ledgerIndex,
    xrpl::LedgerIndex seq,
    uint64_t ownerNode,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
)
{
    xrpl::STObject object(xrpl::sfLedgerEntry);
    object.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(ledgerIndex));
    object.setAccountID(xrpl::sfOwner, getAccountIdWithString(accountId));
    object.setFieldU32(xrpl::sfSequence, seq);
    object.setFieldArray(xrpl::sfAcceptedCredentials, xrpl::STArray{});
    object.setFieldU64(xrpl::sfOwnerNode, ownerNode);
    object.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    object.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);
    object.setFieldU32(xrpl::sfFlags, 0);
    object.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltPERMISSIONED_DOMAIN);

    return object;
}

xrpl::STObject
createDelegateObject(
    std::string_view accountId,
    std::string_view authorize,
    std::string_view ledgerIndex,
    uint64_t ownerNode,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
)
{
    xrpl::STObject object(xrpl::sfLedgerEntry);

    object.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(ledgerIndex));
    object.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltDELEGATE);
    object.setAccountID(xrpl::sfAccount, getAccountIdWithString(accountId));
    object.setAccountID(xrpl::sfAuthorize, getAccountIdWithString(authorize));
    object.setFieldArray(xrpl::sfPermissions, xrpl::STArray{});
    object.setFieldU64(xrpl::sfOwnerNode, ownerNode);
    object.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    object.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);
    object.setFieldU32(xrpl::sfFlags, 0);

    return object;
}

xrpl::STObject
createOraclePriceData(
    uint64_t assetPrice,
    xrpl::Currency baseAssetCurrency,
    xrpl::Currency quoteAssetCurrency,
    uint8_t scale
)
{
    auto priceData = xrpl::STObject(xrpl::sfPriceData);
    priceData.setFieldU64(xrpl::sfAssetPrice, assetPrice);
    priceData.setFieldCurrency(
        xrpl::sfBaseAsset, xrpl::STCurrency{xrpl::sfBaseAsset, baseAssetCurrency}
    );
    priceData.setFieldCurrency(
        xrpl::sfQuoteAsset, xrpl::STCurrency{xrpl::sfQuoteAsset, quoteAssetCurrency}
    );
    priceData.setFieldU8(xrpl::sfScale, scale);

    return priceData;
}

xrpl::STArray
createPriceDataSeries(std::vector<xrpl::STObject> const& series)
{
    return xrpl::STArray{series.begin(), series.end()};
}

xrpl::STObject
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
)
{
    auto ledgerObject = xrpl::STObject(xrpl::sfLedgerEntry);
    ledgerObject.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltORACLE);
    ledgerObject.setFieldU32(xrpl::sfFlags, 0);
    ledgerObject.setAccountID(xrpl::sfOwner, getAccountIdWithString(accountId));
    ledgerObject.setFieldVL(xrpl::sfProvider, xrpl::Blob{provider.begin(), provider.end()});
    ledgerObject.setFieldU64(xrpl::sfOwnerNode, ownerNode);
    ledgerObject.setFieldU32(xrpl::sfLastUpdateTime, lastUpdateTime);
    ledgerObject.setFieldVL(xrpl::sfURI, uri);
    ledgerObject.setFieldVL(xrpl::sfAssetClass, assetClass);
    ledgerObject.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);
    ledgerObject.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    ledgerObject.setFieldArray(xrpl::sfPriceDataSeries, priceDataSeries);

    return ledgerObject;
}

// acc2 issue credential for acc1 so acc2 is issuer
xrpl::STObject
createCredentialObject(
    std::string_view acc1,
    std::string_view acc2,
    std::string_view credType,
    bool accept,
    std::optional<uint32_t> expiration
)
{
    xrpl::STObject credObj(xrpl::sfCredential);
    credObj.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltCREDENTIAL);
    credObj.setFieldVL(xrpl::sfCredentialType, xrpl::Blob{credType.begin(), credType.end()});
    credObj.setAccountID(xrpl::sfSubject, getAccountIdWithString(acc1));
    credObj.setAccountID(xrpl::sfIssuer, getAccountIdWithString(acc2));
    if (expiration.has_value())
        credObj.setFieldU32(xrpl::sfExpiration, *expiration);

    if (accept) {
        credObj.setFieldU32(xrpl::sfFlags, xrpl::lsfAccepted);
    } else {
        credObj.setFieldU32(xrpl::sfFlags, 0);
    }
    credObj.setFieldU64(xrpl::sfSubjectNode, 0);
    credObj.setFieldU64(xrpl::sfIssuerNode, 0);
    credObj.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    credObj.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);
    return credObj;
}

xrpl::STArray
createAuthCredentialArray(
    std::vector<std::string_view> issuer,
    std::vector<std::string_view> credType
)
{
    xrpl::STArray arr;
    ASSERT(issuer.size() == credType.size(), "issuer and credtype vector must be same length");
    for (std::size_t i = 0; i < issuer.size(); ++i) {
        auto credential = xrpl::STObject::makeInnerObject(xrpl::sfCredential);
        credential.setAccountID(xrpl::sfIssuer, getAccountIdWithString(issuer[i]));
        credential.setFieldVL(
            xrpl::sfCredentialType,
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            *xrpl::strUnHex(std::string(credType[i]))
        );
        arr.push_back(credential);
    }
    return arr;
}

xrpl::STObject
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
)
{
    auto vault = xrpl::STObject(xrpl::sfLedgerEntry);
    vault.setAccountID(xrpl::sfOwner, getAccountIdWithString(owner));
    vault.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    vault.setFieldU32(xrpl::sfSequence, seq);
    vault.setFieldU64(xrpl::sfOwnerNode, ownerNode);
    vault.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    vault.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);

    vault.setFieldIssue(
        xrpl::sfAsset, xrpl::STIssue{xrpl::sfAsset, getIssue(assetCurrency, assetIssuer)}
    );
    vault[xrpl::sfShareMPTID] = shareMPTID;
    vault.setFieldNumber(xrpl::sfAssetsTotal, xrpl::STNumber{xrpl::sfAssetsTotal, 300});
    vault.setFieldNumber(xrpl::sfAssetsAvailable, xrpl::STNumber{xrpl::sfAssetsAvailable, 300});
    vault.setFieldNumber(xrpl::sfLossUnrealized, xrpl::STNumber{xrpl::sfLossUnrealized, 1});
    vault.setFieldU8(xrpl::sfWithdrawalPolicy, 200);

    vault.setFieldU32(xrpl::sfFlags, 0);
    vault.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltVAULT);

    return vault;
}

xrpl::STObject
createLoanBroker(
    std::string_view owner,
    std::string_view account,
    xrpl::LedgerIndex seq,
    xrpl::uint256 vaultID,
    uint32_t loanSequence,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
)
{
    auto loanBroker = xrpl::STObject(xrpl::sfLedgerEntry);
    loanBroker.setAccountID(xrpl::sfOwner, getAccountIdWithString(owner));
    loanBroker.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));
    loanBroker.setFieldU32(xrpl::sfSequence, seq);
    loanBroker.setFieldU64(xrpl::sfOwnerNode, 0);
    loanBroker.setFieldU64(xrpl::sfVaultNode, 0);
    loanBroker.setFieldH256(xrpl::sfVaultID, vaultID);
    loanBroker.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    loanBroker.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);
    loanBroker.setFieldU32(xrpl::sfLoanSequence, loanSequence);

    // Optional/default fields - not setting them as they will use default values

    loanBroker.setFieldU32(xrpl::sfFlags, 0);
    loanBroker.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltLOAN_BROKER);

    return loanBroker;
}

xrpl::STObject
createLoan(
    std::string_view borrower,
    xrpl::uint256 loanBrokerID,
    uint32_t loanSequence,
    uint32_t startDate,
    uint32_t paymentInterval,
    int64_t periodicPaymentValue,
    xrpl::uint256 previousTxId,
    uint32_t previousTxSeq
)
{
    auto loan = xrpl::STObject(xrpl::sfLedgerEntry);
    loan.setAccountID(xrpl::sfBorrower, getAccountIdWithString(borrower));
    loan.setFieldH256(xrpl::sfLoanBrokerID, loanBrokerID);
    loan.setFieldU32(xrpl::sfLoanSequence, loanSequence);
    loan.setFieldU64(xrpl::sfOwnerNode, 0);
    loan.setFieldU64(xrpl::sfLoanBrokerNode, 0);
    loan.setFieldH256(xrpl::sfPreviousTxnID, previousTxId);
    loan.setFieldU32(xrpl::sfPreviousTxnLgrSeq, previousTxSeq);

    loan.setFieldU32(xrpl::sfStartDate, startDate);
    loan.setFieldU32(xrpl::sfPaymentInterval, paymentInterval);

    loan.setFieldNumber(
        xrpl::sfPeriodicPayment, xrpl::STNumber{xrpl::sfPeriodicPayment, periodicPaymentValue}
    );

    // Optional/default fields - not setting them as they will use default values

    loan.setFieldU32(xrpl::sfFlags, 0);
    loan.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltLOAN);

    return loan;
}
