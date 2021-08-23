
#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doServerInfo(Context const& context)
{
    boost::json::object response = {};

    auto range = context.backend->fetchLedgerRange();
    if (!range)
    {
        return Status{Error::rpcNOT_READY, "rangeNotFound"};
    }
    else
    {
        response["info"] = boost::json::object{};
        response["info"].as_object()["complete_ledgers"] =
            std::to_string(range->minSequence) + " - " +
            std::to_string(range->maxSequence);
    }
    return response;
}
}  // namespace RPC
