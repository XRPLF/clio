
#include <clio/backend/BackendInterface.h>
#include <clio/rpc/RPCHelpers.h>

namespace RPC {

Result
doLedgerRange(Context const& context)
{
    boost::json::object response = {};

    auto range = context.app.backend().fetchLedgerRange();
    if (!range)
    {
        return Status{Error::rpcNOT_READY, "rangeNotFound"};
    }
    else
    {
        response[JS(ledger_index_min)] = range->minSequence;
        response[JS(ledger_index_max)] = range->maxSequence;
    }

    return response;
}

}  // namespace RPC
