//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

namespace RPC {

Result
doTransactionEntry(Context const& context)
{
    boost::json::object response;
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::uint256 hash;
    if (!hash.parseHex(getRequiredString(context.params, JS(tx_hash))))
        return Status{RippledError::rpcINVALID_PARAMS, "malformedTransaction"};

    auto dbResponse = context.backend->fetchTransaction(hash, context.yield);
    // Note: transaction_entry is meant to only search a specified ledger for
    // the specified transaction. tx searches the entire range of history. For
    // rippled, having two separate commands made sense, as tx would use SQLite
    // and transaction_entry used the nodestore. For clio though, there is no
    // difference between the implementation of these two, as clio only stores
    // transactions in a transactions table, where the key is the hash. However,
    // the API for transaction_entry says the method only searches the specified
    // ledger; we simulate that here by returning not found if the transaction
    // is in a different ledger than the one specified.
    if (!dbResponse || dbResponse->ledgerSequence != lgrInfo.seq)
        return Status{RippledError::rpcTXN_NOT_FOUND, "transactionNotFound", "Transaction not found."};

    auto [txn, meta] = toExpandedJson(*dbResponse);
    response[JS(tx_json)] = std::move(txn);
    response[JS(metadata)] = std::move(meta);
    response[JS(ledger_index)] = lgrInfo.seq;
    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    return response;
}

}  // namespace RPC
