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

#include <rpc/handlers/AccountTx.h>
#include <util/JsonUtils.h>
#include <util/Profiler.h>

namespace rpc {

// found here : https://xrpl.org/transaction-types.html
std::unordered_map<std::string, ripple::TxType> const AccountTxHandler::TYPESMAP{
    {JSL(AccountSet), ripple::ttACCOUNT_SET},
    {JSL(AccountDelete), ripple::ttACCOUNT_DELETE},
    {JSL(AMMBid), ripple::ttAMM_BID},
    {JSL(AMMCreate), ripple::ttAMM_CREATE},
    {JSL(AMMDelete), ripple::ttAMM_DELETE},
    {JSL(AMMDeposit), ripple::ttAMM_DEPOSIT},
    {JSL(AMMVote), ripple::ttAMM_VOTE},
    {JSL(AMMWithdraw), ripple::ttAMM_WITHDRAW},
    {JSL(CheckCancel), ripple::ttCHECK_CANCEL},
    {JSL(CheckCash), ripple::ttCHECK_CASH},
    {JSL(CheckCreate), ripple::ttCHECK_CREATE},
    {JSL(Clawback), ripple::ttCLAWBACK},
    {JSL(DepositPreauth), ripple::ttDEPOSIT_PREAUTH},
    {JSL(EscrowCancel), ripple::ttESCROW_CANCEL},
    {JSL(EscrowCreate), ripple::ttESCROW_CREATE},
    {JSL(EscrowFinish), ripple::ttESCROW_FINISH},
    {JSL(NFTokenAcceptOffer), ripple::ttNFTOKEN_ACCEPT_OFFER},
    {JSL(NFTokenBurn), ripple::ttNFTOKEN_BURN},
    {JSL(NFTokenCancelOffer), ripple::ttNFTOKEN_CANCEL_OFFER},
    {JSL(NFTokenCreateOffer), ripple::ttNFTOKEN_CREATE_OFFER},
    {JSL(NFTokenMint), ripple::ttNFTOKEN_MINT},
    {JSL(OfferCancel), ripple::ttOFFER_CANCEL},
    {JSL(OfferCreate), ripple::ttOFFER_CREATE},
    {JSL(Payment), ripple::ttPAYMENT},
    {JSL(PaymentChannelClaim), ripple::ttPAYCHAN_CLAIM},
    {JSL(PaymentChannelCreate), ripple::ttCHECK_CREATE},
    {JSL(PaymentChannelFund), ripple::ttPAYCHAN_FUND},
    {JSL(SetRegularKey), ripple::ttREGULAR_KEY_SET},
    {JSL(SignerListSet), ripple::ttSIGNER_LIST_SET},
    {JSL(TicketCreate), ripple::ttTICKET_CREATE},
    {JSL(TrustSet), ripple::ttTRUST_SET},
    {JSL(DIDSet), ripple::ttDID_SET},
    {JSL(DIDDelete), ripple::ttDID_DELETE},
};

// TODO: should be std::views::keys when clang supports it
std::unordered_set<std::string> const AccountTxHandler::TYPES_KEYS = [] {
    std::unordered_set<std::string> keys;
    std::transform(TYPESMAP.begin(), TYPESMAP.end(), std::inserter(keys, keys.begin()), [](auto const& pair) {
        return pair.first;
    });
    return keys;
}();

// TODO: this is currently very similar to nft_history but its own copy for time
// being. we should aim to reuse common logic in some way in the future.
AccountTxHandler::Result
AccountTxHandler::process(AccountTxHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto [minIndex, maxIndex] = *range;

    if (input.ledgerIndexMin) {
        if (ctx.apiVersion > 1u &&
            (input.ledgerIndexMin > range->maxSequence || input.ledgerIndexMin < range->minSequence)) {
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMinOutOfRange"}};
        }

        if (static_cast<std::uint32_t>(*input.ledgerIndexMin) > minIndex)
            minIndex = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax) {
        if (ctx.apiVersion > 1u &&
            (input.ledgerIndexMax > range->maxSequence || input.ledgerIndexMax < range->minSequence)) {
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMaxOutOfRange"}};
        }

        if (static_cast<std::uint32_t>(*input.ledgerIndexMax) < maxIndex)
            maxIndex = *input.ledgerIndexMax;
    }

    if (minIndex > maxIndex) {
        if (ctx.apiVersion == 1u)
            return Error{Status{RippledError::rpcLGR_IDXS_INVALID}};

        return Error{Status{RippledError::rpcINVALID_LGR_RANGE}};
    }

    if (input.ledgerHash || input.ledgerIndex || input.usingValidatedLedger) {
        if (ctx.apiVersion > 1u && (input.ledgerIndexMax || input.ledgerIndexMin))
            return Error{Status{RippledError::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"}};

        if (!input.ledgerIndexMax && !input.ledgerIndexMin) {
            // mimic rippled, when both range and index specified, respect the range.
            // take ledger from ledgerHash or ledgerIndex only when range is not specified
            auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
                *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
            );

            if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
                return Error{*status};

            maxIndex = minIndex = std::get<ripple::LedgerHeader>(lgrInfoOrStatus).seq;
        }
    }

    std::optional<data::TransactionsCursor> cursor;

    // if marker exists
    if (input.marker) {
        cursor = {input.marker->ledger, input.marker->seq};
    } else {
        // if forward, start at minIndex - 1, because the SQL query is exclusive, we need to include the 0
        // transaction index of minIndex
        if (input.forward) {
            cursor = {minIndex - 1, std::numeric_limits<int32_t>::max()};
        } else {
            cursor = {maxIndex, std::numeric_limits<int32_t>::max()};
        }
    }

    auto const limit = input.limit.value_or(LIMIT_DEFAULT);
    auto const accountID = accountFromStringStrict(input.account);
    auto const [txnsAndCursor, timeDiff] = util::timed([&]() {
        return sharedPtrBackend_->fetchAccountTransactions(*accountID, limit, input.forward, cursor, ctx.yield);
    });

    LOG(log_.info()) << "db fetch took " << timeDiff << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

    auto const [blobs, retCursor] = txnsAndCursor;
    Output response;

    if (retCursor)
        response.marker = {retCursor->ledgerSequence, retCursor->transactionIndex};

    for (auto const& txnPlusMeta : blobs) {
        // over the range
        if ((txnPlusMeta.ledgerSequence < minIndex && !input.forward) ||
            (txnPlusMeta.ledgerSequence > maxIndex && input.forward)) {
            response.marker = std::nullopt;
            break;
        }
        if (txnPlusMeta.ledgerSequence > maxIndex && !input.forward) {
            LOG(log_.debug()) << "Skipping over transactions from incomplete ledger";
            continue;
        }

        boost::json::object obj;

        // if binary is true or transactionType is specified, we need to expand the transaction
        if (!input.binary || input.transactionType.has_value()) {
            auto [txn, meta] = toExpandedJson(txnPlusMeta, ctx.apiVersion, NFTokenjson::ENABLE);

            if (txn.contains(JS(TransactionType))) {
                auto const objTransactionType = txn[JS(TransactionType)];
                auto const strType = util::toLower(objTransactionType.as_string().c_str());

                // if transactionType does not match
                if (input.transactionType.has_value() && AccountTxHandler::TYPESMAP.contains(strType) &&
                    AccountTxHandler::TYPESMAP.at(strType) != input.transactionType.value())
                    continue;
            }

            if (!input.binary) {
                auto const txKey = ctx.apiVersion < 2u ? JS(tx) : JS(tx_json);
                obj[JS(meta)] = std::move(meta);
                obj[txKey] = std::move(txn);
                obj[txKey].as_object()[JS(date)] = txnPlusMeta.date;
                obj[txKey].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;

                if (ctx.apiVersion < 2u) {
                    obj[txKey].as_object()[JS(inLedger)] = txnPlusMeta.ledgerSequence;
                } else {
                    obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
                    if (obj[txKey].as_object().contains(JS(hash))) {
                        obj[JS(hash)] = obj[txKey].as_object()[JS(hash)];
                        obj[txKey].as_object().erase(JS(hash));
                    }
                    if (auto const ledgerInfo =
                            sharedPtrBackend_->fetchLedgerBySequence(txnPlusMeta.ledgerSequence, ctx.yield);
                        ledgerInfo) {
                        obj[JS(ledger_hash)] = ripple::strHex(ledgerInfo->hash);
                        obj[JS(close_time_iso)] = ripple::to_string_iso(ledgerInfo->closeTime);
                    }
                }
                obj[JS(validated)] = true;
                response.transactions.push_back(std::move(obj));
                continue;
            }
        }
        // binary is true
        obj = toJsonWithBinaryTx(txnPlusMeta, ctx.apiVersion);
        obj[JS(validated)] = true;
        obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
        response.transactions.push_back(std::move(obj));
    }

    response.limit = input.limit;
    response.account = ripple::to_string(*accountID);
    response.ledgerIndexMin = minIndex;
    response.ledgerIndexMax = maxIndex;

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountTxHandler::Output const& output)
{
    jv = {
        {JS(account), output.account},
        {JS(ledger_index_min), output.ledgerIndexMin},
        {JS(ledger_index_max), output.ledgerIndexMax},
        {JS(transactions), output.transactions},
        {JS(validated), output.validated},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

    if (output.limit)
        jv.as_object()[JS(limit)] = *(output.limit);
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountTxHandler::Marker const& marker)
{
    jv = {
        {JS(ledger), marker.ledger},
        {JS(seq), marker.seq},
    };
}

AccountTxHandler::Input
tag_invoke(boost::json::value_to_tag<AccountTxHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountTxHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = jsonObject.at(JS(account)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index_min)) && jsonObject.at(JS(ledger_index_min)).as_int64() != -1)
        input.ledgerIndexMin = jsonObject.at(JS(ledger_index_min)).as_int64();

    if (jsonObject.contains(JS(ledger_index_max)) && jsonObject.at(JS(ledger_index_max)).as_int64() != -1)
        input.ledgerIndexMax = jsonObject.at(JS(ledger_index_max)).as_int64();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        } else {
            // could not get the latest validated ledger seq here, using this flag to indicate that
            input.usingValidatedLedger = true;
        }
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = boost::json::value_to<JsonBool>(jsonObject.at(JS(binary)));

    if (jsonObject.contains(JS(forward)))
        input.forward = boost::json::value_to<JsonBool>(jsonObject.at(JS(forward)));

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker))) {
        input.marker = AccountTxHandler::Marker{
            jsonObject.at(JS(marker)).as_object().at(JS(ledger)).as_int64(),
            jsonObject.at(JS(marker)).as_object().at(JS(seq)).as_int64()
        };
    }

    if (jsonObject.contains("tx_type")) {
        auto objTransactionType = jsonObject.at("tx_type");
        input.transactionType = AccountTxHandler::TYPESMAP.at(objTransactionType.as_string().c_str());
    }

    return input;
}

}  // namespace rpc
