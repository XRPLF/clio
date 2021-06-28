#include <handlers/methods/Ledger.h>
#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>

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