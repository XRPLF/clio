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
#include <util/Profiler.h>

namespace rpc {

// found here : https://xrpl.org/transaction-types.html
std::unordered_map<std::string_view, ripple::TxType> const AccountTxHandler::TYPESMAP{
    {JS(AccountSet), ripple::ttACCOUNT_SET},
    {JS(AccountDelete), ripple::ttACCOUNT_DELETE},
    {JS(CheckCancel), ripple::ttCHECK_CANCEL},
    {JS(CheckCash), ripple::ttCHECK_CASH},
    {JS(CheckCreate), ripple::ttCHECK_CREATE},
    {JS(DepositPreauth), ripple::ttDEPOSIT_PREAUTH},
    {JS(EscrowCancel), ripple::ttESCROW_CANCEL},
    {JS(EscrowCreate), ripple::ttESCROW_CREATE},
    {JS(EscrowFinish), ripple::ttESCROW_FINISH},
    {JS(NFTokenAcceptOffer), ripple::ttNFTOKEN_ACCEPT_OFFER},
    {JS(NFTokenBurn), ripple::ttNFTOKEN_BURN},
    {JS(NFTokenCancelOffer), ripple::ttNFTOKEN_CANCEL_OFFER},
    {JS(NFTokenCreateOffer), ripple::ttNFTOKEN_CREATE_OFFER},
    {JS(NFTokenMint), ripple::ttNFTOKEN_MINT},
    {JS(OfferCancel), ripple::ttOFFER_CANCEL},
    {JS(OfferCreate), ripple::ttOFFER_CREATE},
    {JS(Payment), ripple::ttPAYMENT},
    {JS(PaymentChannelClaim), ripple::ttPAYCHAN_CLAIM},
    {JS(PaymentChannelCreate), ripple::ttCHECK_CREATE},
    {JS(PaymentChannelFund), ripple::ttPAYCHAN_FUND},
    {JS(SetRegularKey), ripple::ttREGULAR_KEY_SET},
    {JS(SignerListSet), ripple::ttSIGNER_LIST_SET},
    {JS(TicketCreate), ripple::ttTICKET_CREATE},
    {JS(TrustSet), ripple::ttTRUST_SET},
};

// TODO: should be std::views::keys when clang supports it
std::unordered_set<std::string_view> const AccountTxHandler::TYPES_KEYS = [] {
    std::unordered_set<std::string_view> keys;
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

    if (input.ledgerIndexMin)
    {
        if (range->maxSequence < input.ledgerIndexMin || range->minSequence > input.ledgerIndexMin)
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMinOutOfRange"}};

        minIndex = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax)
    {
        if (range->maxSequence < input.ledgerIndexMax || range->minSequence > input.ledgerIndexMax)
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMaxOutOfRange"}};

        maxIndex = *input.ledgerIndexMax;
    }

    if (minIndex > maxIndex)
        return Error{Status{RippledError::rpcINVALID_LGR_RANGE}};

    if (input.ledgerHash || input.ledgerIndex || input.usingValidatedLedger)
    {
        // rippled does not have this check
        if (input.ledgerIndexMax || input.ledgerIndexMin)
            return Error{Status{RippledError::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"}};

        auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
            *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

        if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
            return Error{*status};

        maxIndex = minIndex = std::get<ripple::LedgerHeader>(lgrInfoOrStatus).seq;
    }

    std::optional<data::TransactionsCursor> cursor;

    // if marker exists
    if (input.marker)
    {
        cursor = {input.marker->ledger, input.marker->seq};
    }
    else
    {
        // if forward, start at minIndex - 1, because the SQL query is exclusive, we need to include the 0 transaction
        // index of minIndex
        if (input.forward)
            cursor = {minIndex - 1, std::numeric_limits<int32_t>::max()};
        else
            cursor = {maxIndex, std::numeric_limits<int32_t>::max()};
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

    for (auto const& txnPlusMeta : blobs)
    {
        // over the range
        if ((txnPlusMeta.ledgerSequence < minIndex && !input.forward) ||
            (txnPlusMeta.ledgerSequence > maxIndex && input.forward))
        {
            response.marker = std::nullopt;
            break;
        }
        else if (txnPlusMeta.ledgerSequence > maxIndex && !input.forward)
        {
            LOG(log_.debug()) << "Skipping over transactions from incomplete ledger";
            continue;
        }

        boost::json::object obj;
        if (!input.binary)
        {
            auto [txn, meta] = toExpandedJson(txnPlusMeta, NFTokenjson::ENABLE);
            obj[JS(meta)] = std::move(meta);
            obj[JS(tx)] = std::move(txn);

            auto objTransactionType = obj[JS(tx)].as_object()[JS(TransactionType)];
            // if transactionType does not match
            if (input.transactionType.has_value() &&
                AccountTxHandler::TYPESMAP.at(objTransactionType.as_string()) != input.transactionType.value())
                continue;

            obj[JS(tx)].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            obj[JS(tx)].as_object()[JS(date)] = txnPlusMeta.date;
        }
        else
        {
            obj[JS(meta)] = ripple::strHex(txnPlusMeta.metadata);
            obj[JS(tx_blob)] = ripple::strHex(txnPlusMeta.transaction);
            obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
        }

        obj[JS(validated)] = true;

        response.transactions.push_back(obj);
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

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        else
            // could not get the latest validated ledger seq here, using this flag to indicate that
            input.usingValidatedLedger = true;
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = jsonObject.at(JS(binary)).as_bool();

    if (jsonObject.contains(JS(forward)))
        input.forward = jsonObject.at(JS(forward)).as_bool();

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = AccountTxHandler::Marker{
            jsonObject.at(JS(marker)).as_object().at(JS(ledger)).as_int64(),
            jsonObject.at(JS(marker)).as_object().at(JS(seq)).as_int64()};

    if (jsonObject.contains(JS(TransactionType)))
    {
        auto objTransactionType = jsonObject.at(JS(TransactionType));
        input.transactionType = AccountTxHandler::TYPESMAP.at(objTransactionType.as_string());
    }

    return input;
}

}  // namespace rpc
