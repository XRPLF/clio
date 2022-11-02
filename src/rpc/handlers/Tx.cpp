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
        return Status{Error::rpcINVALID_PARAMS, "specifyTransaction"};

    if (!request.at(JS(transaction)).is_string())
        return Status{Error::rpcINVALID_PARAMS, "transactionNotString"};

    ripple::uint256 hash;
    if (!hash.parseHex(request.at(JS(transaction)).as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "malformedTransaction"};

    bool binary = false;
    if (request.contains(JS(binary)))
    {
        if (!request.at(JS(binary)).is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at(JS(binary)).as_bool();
    }

    auto range = context.backend->fetchLedgerRange();
    if (!range)
        return Status{Error::rpcNOT_READY};

    auto dbResponse = context.backend->fetchTransaction(hash, context.yield);
    if (!dbResponse)
        return Status{Error::rpcTXN_NOT_FOUND};

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
