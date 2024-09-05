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

#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"
#include "web/Context.hpp"

#include <xrpl/protocol/ErrorCodes.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace rpc::impl {

template <typename LoadBalancerType, typename CountersType, typename HandlerProviderType>
class ForwardingProxy {
    util::Logger log_{"RPC"};

    std::shared_ptr<LoadBalancerType> balancer_;
    std::reference_wrapper<CountersType> counters_;
    std::shared_ptr<HandlerProviderType const> handlerProvider_;

public:
    ForwardingProxy(
        std::shared_ptr<LoadBalancerType> const& balancer,
        CountersType& counters,
        std::shared_ptr<HandlerProviderType const> const& handlerProvider
    )
        : balancer_{balancer}, counters_{std::ref(counters)}, handlerProvider_{handlerProvider}
    {
    }

    bool
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

        auto const checkAccountInfoForward = [&]() {
            return ctx.method == "account_info" and request.contains("queue") and request.at("queue").is_bool() and
                request.at("queue").as_bool();
        };

        auto const checkLedgerForward = [&]() {
            return ctx.method == "ledger" and request.contains("queue") and request.at("queue").is_bool() and
                request.at("queue").as_bool();
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

    bool
    isProxied(std::string const& method) const
    {
        static std::unordered_set<std::string> const proxiedCommands{
            "server_definitions",
            "server_state",
            "submit",
            "submit_multisigned",
            "fee",
            "ledger_closed",
            "ledger_current",
            "ripple_path_find",
            "manifest",
            "channel_authorize",
            "channel_verify",
        };

        return proxiedCommands.contains(method);
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

    bool
    validHandler(std::string const& method) const
    {
        return handlerProvider_->contains(method) || isProxied(method);
    }
};

}  // namespace rpc::impl
