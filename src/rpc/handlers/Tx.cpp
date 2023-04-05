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

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

// {
//   transaction: <hex>
// }

Result
doTx(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains(JS(transaction)))
        return Status{RippledError::rpcINVALID_PARAMS, "specifyTransaction"};

    if (!request.at(JS(transaction)).is_string())
        return Status{RippledError::rpcINVALID_PARAMS, "transactionNotString"};

    ripple::uint256 hash;
    if (!hash.parseHex(request.at(JS(transaction)).as_string().c_str()))
        return Status{RippledError::rpcINVALID_PARAMS, "malformedTransaction"};

    bool binary = false;
    if (request.contains(JS(binary)))
    {
        if (!request.at(JS(binary)).is_bool())
            return Status{RippledError::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at(JS(binary)).as_bool();
    }
    auto minLedger = getUInt(request, JS(min_ledger));
    auto maxLedger = getUInt(request, JS(max_ledger));
    bool rangeSupplied = minLedger && maxLedger;

    if (rangeSupplied)
    {
        if (*minLedger > *maxLedger)
            return Status{RippledError::rpcINVALID_LGR_RANGE};
        if (*maxLedger - *minLedger > 1000)
            return Status{RippledError::rpcEXCESSIVE_LGR_RANGE};
    }

    auto range = context.backend->fetchLedgerRange();
    if (!range)
        return Status{RippledError::rpcNOT_READY};

    auto dbResponse = context.backend->fetchTransaction(hash, context.yield);
    if (!dbResponse)
    {
        if (rangeSupplied)
        {
            bool searchedAll = range->maxSequence >= *maxLedger && range->minSequence <= *minLedger;
            boost::json::object extra;
            extra["searched_all"] = searchedAll;
            return Status{RippledError::rpcTXN_NOT_FOUND, std::move(extra)};
        }
        return Status{RippledError::rpcTXN_NOT_FOUND};
    }

    if (!binary)
    {
        auto [txn, meta] = toExpandedJson(*dbResponse);
        response = txn;
        response[JS(meta)] = meta;
    }
    else
    {
        response[JS(tx)] = ripple::strHex(dbResponse->transaction);
        response[JS(meta)] = ripple::strHex(dbResponse->metadata);
        response[JS(hash)] = std::move(request.at(JS(transaction)).as_string());
    }
    response[JS(date)] = dbResponse->date;
    response[JS(ledger_index)] = dbResponse->ledgerSequence;

    return response;
}

}  // namespace RPC
