#include <rpc/handlers/Ledger.h>
#include <rpc/RPCHelpers.h>
#include <reporting/BackendInterface.h>

namespace RPC
{
    
Status
LedgerRange::check()
{
    auto range = context_.backend->fetchLedgerRange();
    if (!range)
    {
        return {Error::rpcNOT_READY, "rangeNotFound"};
    }
    else
    {
        response_["ledger_index_min"] = range->minSequence;
        response_["ledger_index_max"] = range->maxSequence;
    }

    return OK;
}

}