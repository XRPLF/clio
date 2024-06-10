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

#include "rpc/handlers/TransactionEntry.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/chrono.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/jss.h>

#include <string>
#include <utility>
#include <variant>

namespace rpc {

TransactionEntryHandler::Result
TransactionEntryHandler::process(TransactionEntryHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto output = TransactionEntryHandler::Output{};
    output.apiVersion = ctx.apiVersion;

    output.ledgerHeader = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const dbRet = sharedPtrBackend_->fetchTransaction(ripple::uint256{input.txHash.c_str()}, ctx.yield);
    // Note: transaction_entry is meant to only search a specified ledger for
    // the specified transaction. tx searches the entire range of history. For
    // rippled, having two separate commands made sense, as tx would use SQLite
    // and transaction_entry used the nodestore. For clio though, there is no
    // difference between the implementation of these two, as clio only stores
    // transactions in a transactions table, where the key is the hash. However,
    // the API for transaction_entry says the method only searches the specified
    // ledger; we simulate that here by returning not found if the transaction
    // is in a different ledger than the one specified.
    if (!dbRet || dbRet->ledgerSequence != output.ledgerHeader->seq)
        return Error{Status{RippledError::rpcTXN_NOT_FOUND, "transactionNotFound", "Transaction not found."}};

    auto [txn, meta] = toExpandedJson(*dbRet, ctx.apiVersion);

    output.tx = std::move(txn);
    output.metadata = std::move(meta);

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, TransactionEntryHandler::Output const& output)
{
    auto const metaKey = output.apiVersion > 1u ? JS(meta) : JS(metadata);
    jv = {
        {JS(validated), output.validated},
        {metaKey, output.metadata},
        {JS(tx_json), output.tx},
        {JS(ledger_index), output.ledgerHeader->seq},
        {JS(ledger_hash), ripple::strHex(output.ledgerHeader->hash)},
    };

    if (output.apiVersion > 1u) {
        jv.as_object()[JS(close_time_iso)] = ripple::to_string_iso(output.ledgerHeader->closeTime);
        if (output.tx.contains(JS(hash))) {
            jv.as_object()[JS(hash)] = output.tx.at(JS(hash));
            jv.as_object()[JS(tx_json)].as_object().erase(JS(hash));
        }
    }
}

TransactionEntryHandler::Input
tag_invoke(boost::json::value_to_tag<TransactionEntryHandler::Input>, boost::json::value const& jv)
{
    auto input = TransactionEntryHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.txHash = boost::json::value_to<std::string>(jv.at(JS(tx_hash)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    return input;
}

}  // namespace rpc
