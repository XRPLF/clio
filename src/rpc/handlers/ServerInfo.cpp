
#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <etl/ReportingETL.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doServerInfo(Context const& context)
{
    boost::json::object response = {};

    auto range = context.backend->fetchLedgerRange();
    if (!range)
    {
        return Status{
            Error::rpcNOT_READY,
            "emptyDatabase",
            "The server has no data in the database"};
    }
    else
    {
        response["info"] = boost::json::object{};
        boost::json::object& info = response["info"].as_object();

        info["complete_ledgers"] = std::to_string(range->minSequence) + "-" +
            std::to_string(range->maxSequence);

        info["counters"] = boost::json::object{};
        info["counters"].as_object()["rpc"] = context.counters.report();
    }
    auto& cache = (response["cache"] = boost::json::object{}).as_object();
    cache["size"] = context.backend->cache().size();
    cache["is_full"] = context.backend->cache().isFull();
    cache["latest_ledger_seq"] =
        context.backend->cache().latestLedgerSequence();

    response["etl"] = context.etl->getInfo();

    auto serverInfoRippled = context.balancer->forwardToRippled(
        context.params, context.clientIp, context.yield);

    if (serverInfoRippled && !serverInfoRippled->contains("error"))
        response["info"].as_object()["load_factor"] = 1;

    auto lgrInfo = context.backend->fetchLedgerBySequence(
        range->maxSequence, context.yield);

    assert(lgrInfo.has_value());
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count() -
        lgrInfo->closeTime.time_since_epoch().count() - 946684800;
    auto& validatedLgr =
        (response["validated_ledger"] = boost::json::object{}).as_object();
    validatedLgr["age"] = age;
    validatedLgr["hash"] = ripple::strHex(lgrInfo->hash);
    validatedLgr["seq"] = lgrInfo->seq;
    auto fees = context.backend->fetchFees(lgrInfo->seq, context.yield);
    assert(fees.has_value());
    validatedLgr["base_fee_xrp"] = fees->base.decimalXRP();
    validatedLgr["reserve_base_xrp"] = fees->reserve.decimalXRP();
    validatedLgr["reserve_inc_xrp"] = fees->increment.decimalXRP();

    response["note"] =
        "This is a clio server. If you want to talk to rippled, include "
        "\"ledger_index\":\"current\" in your request";
    return response;
}
}  // namespace RPC
