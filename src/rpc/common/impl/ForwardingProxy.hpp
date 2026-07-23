#pragma once

#include "etl/LoadBalancerInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/RPCCenter.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"
#include "web/Context.hpp"

#include <xrpl/protocol/ErrorCodes.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace rpc::impl {

template <typename CountersType, typename HandlerProviderType>
class ForwardingProxy {
    util::Logger log_{"RPC"};

    std::shared_ptr<etl::LoadBalancerInterface> balancer_;
    std::reference_wrapper<CountersType> counters_;
    std::shared_ptr<HandlerProviderType const> handlerProvider_;

public:
    ForwardingProxy(
        std::shared_ptr<etl::LoadBalancerInterface> const& balancer,
        CountersType& counters,
        std::shared_ptr<HandlerProviderType const> const& handlerProvider
    )
        : balancer_{balancer}, counters_{std::ref(counters)}, handlerProvider_{handlerProvider}
    {
    }

    [[nodiscard]] bool
    shouldForward(web::Context const& ctx) const
    {
        auto const& request = ctx.params;

        if (ctx.method == "subscribe" || ctx.method == "unsubscribe")
            return false;

        if (handlerProvider_->isClioOnly(ctx.method))
            return false;

        if (isProxied(ctx.method))
            return true;

        if (specifiesCurrentOrClosedLedger(request))
            return true;

        if (isForcedForward(ctx))
            return true;

        auto const checkAccountInfoForward = [&]() {
            return ctx.method == "account_info" and request.contains("queue") and
                request.at("queue").is_bool() and request.at("queue").as_bool();
        };

        auto const checkLedgerForward = [&]() {
            return ctx.method == "ledger" and request.contains("queue") and
                request.at("queue").is_bool() and request.at("queue").as_bool();
        };

        return static_cast<bool>(checkAccountInfoForward() or checkLedgerForward());
    }

    Result
    forward(web::Context const& ctx)
    {
        auto toForward = ctx.params;
        toForward["command"] = ctx.method;

        auto res = balancer_->forwardToRippled(toForward, ctx.clientIp, ctx.isAdmin, ctx.yield);
        if (not res) {
            notifyFailedToForward(ctx.method);
            return Result{Status{CombinedError{res.error()}}};
        }

        notifyForwarded(ctx.method);
        return Result{std::move(res).value()};
    }

    [[nodiscard]] bool
    isProxied(std::string const& method) const
    {
        return RPCCenter::isForwarded(method);
    }

private:
    void
    notifyForwarded(std::string const& method)
    {
        if (validHandler(method))
            counters_.get().rpcForwarded(method);
    }

    void
    notifyFailedToForward(std::string const& method)
    {
        if (validHandler(method))
            counters_.get().rpcFailedToForward(method);
    }

    [[nodiscard]] bool
    validHandler(std::string const& method) const
    {
        return handlerProvider_->contains(method) || isProxied(method);
    }

    [[nodiscard]] bool
    isForcedForward(web::Context const& ctx) const
    {
        static constexpr auto kForceForward = "force_forward";
        return ctx.isAdmin and ctx.params.contains(kForceForward) and
            ctx.params.at(kForceForward).is_bool() and ctx.params.at(kForceForward).as_bool();
    }
};

}  // namespace rpc::impl
