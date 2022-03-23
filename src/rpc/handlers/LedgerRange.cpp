
#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doLedgerRange(Context const& context)
{
    boost::json::object response = {};

    auto range = context.backend->fetchLedgerRange();
    if (!range)
    {
        return Status{Error::rpcNOT_READY, "rangeNotFound"};
    }
    else
    {
        response["ledger_index_min"] = range->minSequence;
        response["ledger_index_max"] = range->maxSequence;
    }

    return response;
}

}  // namespace RPC