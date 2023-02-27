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

#include <rpc/RPCHelpers.h>
#include <rpc/ngHandlers/Tx.h>

namespace RPCng {

TxHandler::Result
TxHandler::process(Input input, boost::asio::yield_context& yield) const
{
    auto const rangeSupplied = input.minLedger && input.maxLedger;

    if (rangeSupplied)
    {
        if (*input.minLedger > *input.maxLedger)
            return Error{RPC::Status{RPC::RippledError::rpcINVALID_LGR_RANGE}};
        if (*input.maxLedger - *input.minLedger > 1000)
            return Error{
                RPC::Status{RPC::RippledError::rpcEXCESSIVE_LGR_RANGE}};
    }
    TxHandler::Output output;
    auto const dbResponse = sharedPtrBackend_->fetchTransaction(
        ripple::uint256{std::string_view(input.transaction)}, yield);
    if (!dbResponse)
    {
        if (rangeSupplied)
        {
            auto const range = sharedPtrBackend_->fetchLedgerRange();
            auto const searchedAll = range->maxSequence >= *input.maxLedger &&
                range->minSequence <= *input.minLedger;
            boost::json::object extra;
            extra["searched_all"] = searchedAll;
            return Error{RPC::Status{
                RPC::RippledError::rpcTXN_NOT_FOUND, std::move(extra)}};
        }
        return Error{RPC::Status{RPC::RippledError::rpcTXN_NOT_FOUND}};
    }

    // clio does not implement 'inLedger' which is a deprecated field
    if (!input.binary)
    {
        auto const [txn, meta] = RPC::toExpandedJson(*dbResponse);
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
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    TxHandler::Output output)
{
    auto obj = boost::json::object{};
    if (output.tx)
    {
        obj = *output.tx;
        obj["meta"] = *output.meta;
    }
    else
    {
        obj["meta"] = *output.metaStr;
        obj["tx"] = *output.txStr;
        obj["hash"] = output.hash;
    }
    obj["date"] = output.date;
    obj["ledger_index"] = output.ledgerIndex;
    jv = std::move(obj);
}

TxHandler::Input
tag_invoke(
    boost::json::value_to_tag<TxHandler::Input>,
    boost::json::value const& jv)
{
    TxHandler::Input input;
    auto const& jsonObject = jv.as_object();
    input.transaction = jv.at("transaction").as_string().c_str();
    if (jsonObject.contains("binary"))
    {
        input.binary = jv.at("binary").as_bool();
    }
    if (jsonObject.contains("min_ledger"))
    {
        input.minLedger = jv.at("min_ledger").as_int64();
    }
    if (jsonObject.contains("max_ledger"))
    {
        input.maxLedger = jv.at("max_ledger").as_int64();
    }
    return input;
}

}  // namespace RPCng
