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

namespace RPC {

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
        return Error{Status{RippledError::rpcINVALID_PARAMS, "invalidIndex"}};

    if (input.ledgerHash || input.ledgerIndex)
    {
        // rippled does not have this check
        if (input.ledgerIndexMax || input.ledgerIndexMin)
            return Error{Status{RippledError::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"}};

        auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
            *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

        if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
            return Error{*status};

        maxIndex = minIndex = std::get<ripple::LedgerInfo>(lgrInfoOrStatus).seq;
    }

    std::optional<Backend::TransactionsCursor> cursor;

    // if marker exists
    if (input.marker)
    {
        cursor = {input.marker->ledger, input.marker->seq};
    }
    else
    {
        if (input.forward)
            cursor = {minIndex, 0};
        else
            cursor = {maxIndex, INT32_MAX};
    }

    static auto constexpr limitDefault = 50;
    auto const limit = input.limit.value_or(limitDefault);
    auto const accountID = accountFromStringStrict(input.account);
    auto const [txnsAndCursor, timeDiff] = util::timed([&]() {
        return sharedPtrBackend_->fetchAccountTransactions(*accountID, limit, input.forward, cursor, ctx.yield);
    });

    log_.info() << "db fetch took " << timeDiff << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

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
            log_.debug() << "Skipping over transactions from incomplete ledger";
            continue;
        }

        boost::json::object obj;
        if (!input.binary)
        {
            auto [txn, meta] = toExpandedJson(txnPlusMeta);
            obj[JS(meta)] = std::move(meta);
            obj[JS(tx)] = std::move(txn);
            obj[JS(tx)].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            obj[JS(tx)].as_object()[JS(date)] = txnPlusMeta.date;
        }
        else
        {
            obj[JS(meta)] = ripple::strHex(txnPlusMeta.metadata);
            obj[JS(tx_blob)] = ripple::strHex(txnPlusMeta.transaction);
            obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            // only clio has this field
            obj[JS(date)] = txnPlusMeta.date;
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

    return input;
}

}  // namespace RPC
