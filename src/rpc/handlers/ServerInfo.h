//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <main/Build.h>
#include <rpc/Errors.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <ripple/basics/chrono.h>

#include <chrono>
#include <fmt/core.h>

class SubscriptionManager;
class ETLService;
class LoadBalancer;

namespace RPC {
class Counters;
}

namespace RPC {

template <typename SubscriptionManagerType, typename LoadBalancerType, typename ETLServiceType, typename CountersType>
class BaseServerInfoHandler
{
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;
    std::shared_ptr<LoadBalancerType> balancer_;
    std::shared_ptr<ETLServiceType const> etl_;
    std::reference_wrapper<CountersType const> counters_;

public:
    struct AdminSection
    {
        boost::json::object counters = {};
        boost::json::object subscriptions = {};
        boost::json::object etl = {};
    };

    struct ValidatedLedgerSection
    {
        uint32_t age = 0;
        std::string hash = {};
        ripple::LedgerIndex seq = {};
        std::optional<ripple::Fees> fees = std::nullopt;
    };

    struct CacheSection
    {
        std::size_t size = 0;
        bool isFull = false;
        ripple::LedgerIndex latestLedgerSeq = {};
        float objectHitRate = 1.0;
        float successorHitRate = 1.0;
    };

    struct InfoSection
    {
        std::optional<AdminSection> adminSection = std::nullopt;
        std::string completeLedgers = {};
        uint32_t loadFactor = 1u;
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::chrono::seconds uptime = {};
        std::string clioVersion = Build::getClioVersionString();
        std::optional<boost::json::object> rippledInfo = std::nullopt;
        ValidatedLedgerSection validatedLedger = {};
        CacheSection cache = {};
        bool isAmendmentBlocked = false;
    };

    struct Output
    {
        InfoSection info = {};

        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    BaseServerInfoHandler(
        std::shared_ptr<BackendInterface> const& backend,
        std::shared_ptr<SubscriptionManagerType> const& subscriptions,
        std::shared_ptr<LoadBalancerType> const& balancer,
        std::shared_ptr<ETLServiceType const> const& etl,
        CountersType const& counters)
        : backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , counters_(std::cref(counters))
    {
    }

    Result
    process(Context const& ctx) const
    {
        using namespace RPC;
        using namespace std::chrono;

        auto const range = backend_->fetchLedgerRange();
        auto const lgrInfo = backend_->fetchLedgerBySequence(range->maxSequence, ctx.yield);
        if (not lgrInfo.has_value())
            return Error{Status{RippledError::rpcINTERNAL}};

        auto const fees = backend_->fetchFees(lgrInfo->seq, ctx.yield);
        if (not fees.has_value())
            return Error{Status{RippledError::rpcINTERNAL}};

        auto output = Output{};
        auto const sinceEpoch = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        auto const age = static_cast<int32_t>(sinceEpoch) -
            static_cast<int32_t>(lgrInfo->closeTime.time_since_epoch().count()) -
            static_cast<int32_t>(rippleEpochStart);

        output.info.completeLedgers = fmt::format("{}-{}", range->minSequence, range->maxSequence);

        if (ctx.isAdmin)
            output.info.adminSection = {counters_.get().report(), subscriptions_->report(), etl_->getInfo()};

        auto const serverInfoRippled =
            balancer_->forwardToRippled({{"command", "server_info"}}, ctx.clientIp, ctx.yield);

        if (serverInfoRippled && !serverInfoRippled->contains(JS(error)))
        {
            if (serverInfoRippled->contains(JS(result)) &&
                serverInfoRippled->at(JS(result)).as_object().contains(JS(info)))
            {
                output.info.rippledInfo = serverInfoRippled->at(JS(result)).as_object().at(JS(info)).as_object();
            }
        }

        output.info.validatedLedger.age = age < 0 ? 0 : age;
        output.info.validatedLedger.hash = ripple::strHex(lgrInfo->hash);
        output.info.validatedLedger.seq = lgrInfo->seq;
        output.info.validatedLedger.fees = fees;
        output.info.cache.size = backend_->cache().size();
        output.info.cache.isFull = backend_->cache().isFull();
        output.info.cache.latestLedgerSeq = backend_->cache().latestLedgerSequence();
        output.info.cache.objectHitRate = backend_->cache().getObjectHitRate();
        output.info.cache.successorHitRate = backend_->cache().getSuccessorHitRate();
        output.info.uptime = counters_.get().uptime();
        output.info.isAmendmentBlocked = etl_->isAmendmentBlocked();

        return output;
    }

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = {
            {JS(info), output.info},
            {JS(validated), output.validated},
        };
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, InfoSection const& info)
    {
        using ripple::to_string;

        jv = {
            {JS(complete_ledgers), info.completeLedgers},
            {JS(load_factor), info.loadFactor},
            {JS(time), to_string(std::chrono::floor<std::chrono::microseconds>(info.time))},
            {JS(uptime), info.uptime.count()},
            {"clio_version", info.clioVersion},
            {JS(validated_ledger), info.validatedLedger},
            {"cache", info.cache},
        };

        if (info.isAmendmentBlocked)
            jv.as_object()[JS(amendment_blocked)] = true;

        if (info.rippledInfo)
        {
            try
            {
                auto const& rippledInfo = info.rippledInfo.value();

                jv.as_object()[JS(load_factor)] = rippledInfo.at(JS(load_factor));
                jv.as_object()[JS(validation_quorum)] = rippledInfo.at(JS(validation_quorum));
                jv.as_object()["rippled_version"] = rippledInfo.at(JS(build_version));
                jv.as_object()[JS(network_id)] = rippledInfo.at(JS(network_id));
            }
            catch (std::exception const&)
            {
            }
        }

        if (info.adminSection)
        {
            jv.as_object()["etl"] = info.adminSection->etl;
            jv.as_object()[JS(counters)] = info.adminSection->counters;
            jv.as_object()[JS(counters)].as_object()["subscriptions"] = info.adminSection->subscriptions;
        }
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ValidatedLedgerSection const& validated)
    {
        jv = {
            {JS(age), validated.age},
            {JS(hash), validated.hash},
            {JS(seq), validated.seq},
            {JS(base_fee_xrp), validated.fees->base.decimalXRP()},
            {JS(reserve_base_xrp), validated.fees->reserve.decimalXRP()},
            {JS(reserve_inc_xrp), validated.fees->increment.decimalXRP()},
        };
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, CacheSection const& cache)
    {
        jv = {
            {"size", cache.size},
            {"is_full", cache.isFull},
            {"latest_ledger_seq", cache.latestLedgerSeq},
            {"object_hit_rate", cache.objectHitRate},
            {"successor_hit_rate", cache.successorHitRate},
        };
    }
};

/**
 * @brief The server_info command asks the Clio server for a human-readable version of various information about the
 * Clio server being queried.
 *
 * For more details see: https://xrpl.org/server_info-clio.html
 */
using ServerInfoHandler = BaseServerInfoHandler<SubscriptionManager, LoadBalancer, ETLService, Counters>;

}  // namespace RPC
