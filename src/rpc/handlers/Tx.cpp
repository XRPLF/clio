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

#include <rpc/handlers/Tx.h>

namespace rpc {

TxHandler::Result
TxHandler::process(Input input, Context const& ctx) const
{
    static auto constexpr maxLedgerRange = 1000u;
    auto const rangeSupplied = input.minLedger && input.maxLedger;

    if (rangeSupplied)
    {
        if (*input.minLedger > *input.maxLedger)
            return Error{Status{RippledError::rpcINVALID_LGR_RANGE}};

        if (*input.maxLedger - *input.minLedger > maxLedgerRange)
            return Error{Status{RippledError::rpcEXCESSIVE_LGR_RANGE}};
    }

    auto output = TxHandler::Output{.apiVersion = ctx.apiVersion};
    auto const dbResponse =
        sharedPtrBackend_->fetchTransaction(ripple::uint256{std::string_view(input.transaction)}, ctx.yield);

    if (!dbResponse)
    {
        if (rangeSupplied)
        {
            auto const range = sharedPtrBackend_->fetchLedgerRange();
            auto const searchedAll = range->maxSequence >= *input.maxLedger && range->minSequence <= *input.minLedger;
            boost::json::object extra;
            extra["searched_all"] = searchedAll;

            return Error{Status{RippledError::rpcTXN_NOT_FOUND, std::move(extra)}};
        }

        return Error{Status{RippledError::rpcTXN_NOT_FOUND}};
    }

    if (!input.binary)
    {
        auto const [txn, meta] = toExpandedJson(*dbResponse, NFTokenjson::ENABLE);
        output.tx = txn;
        output.meta = meta;
    }
    else
    {
        output.txStr = ripple::strHex(dbResponse->transaction);
        output.metaStr = ripple::strHex(dbResponse->metadata);
        output.hash = std::move(input.transaction);
    }

    output.date = dbResponse->date;
    output.ledgerIndex = dbResponse->ledgerSequence;

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, TxHandler::Output const& output)
{
    auto obj = boost::json::object{};

    if (output.tx)
    {
        obj = *output.tx;
        obj[JS(meta)] = *output.meta;
    }
    else
    {
        obj[JS(meta)] = *output.metaStr;
        obj[JS(tx)] = *output.txStr;
        obj[JS(hash)] = output.hash;
    }
    obj[JS(validated)] = output.validated;
    obj[JS(date)] = output.date;
    obj[JS(ledger_index)] = output.ledgerIndex;

    if (output.apiVersion < 2)
        obj[JS(inLedger)] = output.ledgerIndex;

    jv = std::move(obj);
}

TxHandler::Input
tag_invoke(boost::json::value_to_tag<TxHandler::Input>, boost::json::value const& jv)
{
    auto input = TxHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.transaction = jv.at(JS(transaction)).as_string().c_str();

    if (jsonObject.contains(JS(binary)))
        input.binary = jv.at(JS(binary)).as_bool();

    if (jsonObject.contains(JS(min_ledger)))
        input.minLedger = jv.at(JS(min_ledger)).as_int64();

    if (jsonObject.contains(JS(max_ledger)))
        input.maxLedger = jv.at(JS(max_ledger)).as_int64();

    return input;
}

}  // namespace rpc
