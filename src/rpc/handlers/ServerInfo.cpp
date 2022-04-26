
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

    auto lgrInfo = context.backend->fetchLedgerBySequence(
        range->maxSequence, context.yield);

    auto fees = context.backend->fetchFees(lgrInfo->seq, context.yield);

    if (!lgrInfo || !fees)
        return Status{Error::rpcINTERNAL};

    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count() -
        lgrInfo->closeTime.time_since_epoch().count() - 946684800;

    if (age < 0)
        age = 0;

    response["info"] = boost::json::object{};
    boost::json::object& info = response["info"].as_object();

    info["complete_ledgers"] = std::to_string(range->minSequence) + "-" +
        std::to_string(range->maxSequence);

    info["counters"] = boost::json::object{};
    info["counters"].as_object()["rpc"] = context.counters.report();

    auto serverInfoRippled = context.balancer->forwardToRippled(
        {{"command", "server_info"}}, context.clientIp, context.yield);
    
    info["load_factor"] = 1;
    if (serverInfoRippled && !serverInfoRippled->contains("error"))
    {
        try
        {
            auto& rippledResult = serverInfoRippled->at("result").as_object();
            auto& rippledInfo = rippledResult.at("info").as_object();
            info["load_factor"] = rippledInfo["load_factor"];
            info["validation_quorum"] = rippledInfo["validation_quorum"];
        }
        catch (std::exception const&) {}
    }

    info["validated_ledger"] = boost::json::object{};
    boost::json::object& validated = info["validated_ledger"].as_object();

    validated["age"] = age;
    validated["hash"] = ripple::strHex(lgrInfo->hash);
    validated["seq"] = lgrInfo->seq;
    validated["base_fee_xrp"] = fees->base.decimalXRP();
    validated["reserve_base_xrp"] = fees->reserve.decimalXRP();
    validated["reserve_inc_xrp"] = fees->increment.decimalXRP();

    response["cache"] = boost::json::object{};
    auto& cache = response["cache"].as_object();

    cache["size"] = context.backend->cache().size();
    cache["is_full"] = context.backend->cache().isFull();
    cache["latest_ledger_seq"] =
        context.backend->cache().latestLedgerSequence();

    response["etl"] = context.etl->getInfo();

    response["note"] =
        "This is a clio server. If you want to talk to rippled, include "
        "\"ledger_index\":\"current\" in your request";
    return response;
}
}  // namespace RPC
