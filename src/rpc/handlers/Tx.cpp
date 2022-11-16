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

    auto range = context.backend->fetchLedgerRange();
    if (!range)
        return Status{RippledError::rpcNOT_READY};

    if (request.contains(JS(min_ledger)) && request.contains(JS(max_ledger)))
    {
        if (request.at(JS(min_ledger)).is_int64() &&
            request.at(JS(max_ledger)).is_int64())
        {
            const auto& minLedger = request.at(JS(min_ledger)).as_int64();
            const auto& maxLedger = request.at(JS(max_ledger)).as_int64();

            if (minLedger >= 0 && maxLedger >= 0 && maxLedger > minLedger)
            {
                if (maxLedger - minLedger > 1000)
                    return Status{
                        RippledError::rpcEXCESSIVE_LGR_RANGE,
                        "excessiveLgrRange"};
            }

            else
                return Status{
                    RippledError::rpcLGR_IDXS_INVALID, "lgrIdxsInvalid"};
        }

        else
            return Status{
                RippledError::rpcLGR_IDX_MALFORMED, "lgrIdxMalformed"};
    }

    auto dbResponse = context.backend->fetchTransaction(hash, context.yield);
    if (!dbResponse)
        return Status{RippledError::rpcTXN_NOT_FOUND};

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
