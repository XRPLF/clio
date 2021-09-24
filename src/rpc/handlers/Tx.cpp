#include <backend/BackendInterface.h>
#include <backend/Pg.h>
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

    if (!request.contains("transaction"))
        return Status{Error::rpcINVALID_PARAMS, "specifyTransaction"};

    if (!request.at("transaction").is_string())
        return Status{Error::rpcINVALID_PARAMS, "transactionNotString"};

    ripple::uint256 hash;
    if (!hash.parseHex(request.at("transaction").as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "malformedTransaction"};

    bool binary = false;
    if (request.contains("binary"))
    {
        if (!request.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at("binary").as_bool();
    }

    auto range = context.backend->fetchLedgerRange();
    if (!range)
        return Status{Error::rpcNOT_READY};

    auto dbResponse = context.backend->fetchTransaction(hash);
    if (!dbResponse)
        return Status{Error::rpcTXN_NOT_FOUND};

    if (!binary)
    {
        auto [txn, meta] = toExpandedJson(*dbResponse);
        response = txn;
        response["meta"] = meta;
    }
    else
    {
        response["tx"] = ripple::strHex(dbResponse->transaction);
        response["meta"] = ripple::strHex(dbResponse->metadata);
        response["hash"] = std::move(request.at("transaction").as_string());
    }
    response["date"] = dbResponse->date;
    response["ledger_index"] = dbResponse->ledgerSequence;

    return response;
}

}  // namespace RPC
