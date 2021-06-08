#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>

boost::json::object
doLedgerRange(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;

    auto range = backend.fetchLedgerRange();
    if (!range)
    {
        response["error"] = "No data";
    }
    else
    {
        response["ledger_index_min"] = range->minSequence;
        response["ledger_index_max"] = range->maxSequence;
    }
    return response;
}
