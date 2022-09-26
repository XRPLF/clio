
#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <etl/ReportingETL.h>
#include <main/Build.h>
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

    response[JS(info)] = boost::json::object{};
    boost::json::object& info = response[JS(info)].as_object();

    info[JS(complete_ledgers)] = std::to_string(range->minSequence) + "-" +
        std::to_string(range->maxSequence);

    bool admin = context.clientIp == "127.0.0.1";

    if (admin)
    {
        info[JS(counters)] = context.counters.report();
        info[JS(counters)].as_object()["subscriptions"] =
            context.subscriptions->report();
    }

    auto serverInfoRippled = context.balancer->forwardToRippled(
        {{"command", "server_info"}}, context.clientIp, context.yield);

    info[JS(load_factor)] = 1;
    info["clio_version"] = Build::getClioVersionString();
    if (serverInfoRippled && !serverInfoRippled->contains(JS(error)))
    {
        try
        {
            auto& rippledResult = serverInfoRippled->at(JS(result)).as_object();
            auto& rippledInfo = rippledResult.at(JS(info)).as_object();
            info[JS(load_factor)] = rippledInfo[JS(load_factor)];
            info[JS(validation_quorum)] = rippledInfo[JS(validation_quorum)];
            info["rippled_version"] = rippledInfo[JS(build_version)];
        }
        catch (std::exception const&)
        {
        }
    }

    info[JS(validated_ledger)] = boost::json::object{};
    boost::json::object& validated = info[JS(validated_ledger)].as_object();

    validated[JS(age)] = age;
    validated[JS(hash)] = ripple::strHex(lgrInfo->hash);
    validated[JS(seq)] = lgrInfo->seq;
    validated[JS(base_fee_xrp)] = fees->base.decimalXRP();
    validated[JS(reserve_base_xrp)] = fees->reserve.decimalXRP();
    validated[JS(reserve_inc_xrp)] = fees->increment.decimalXRP();

    info["cache"] = boost::json::object{};
    auto& cache = info["cache"].as_object();

    cache["size"] = context.backend->cache().size();
    cache["is_full"] = context.backend->cache().isFull();
    cache["latest_ledger_seq"] =
        context.backend->cache().latestLedgerSequence();
    cache["object_hit_rate"] = context.backend->cache().getObjectHitRate();
    cache["successor_hit_rate"] =
        context.backend->cache().getSuccessorHitRate();

    if (admin)
    {
        info["etl"] = context.etl->getInfo();
    }

    return response;
}
}  // namespace RPC
