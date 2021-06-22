//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2021 Ripple Labs Inc.
    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.
    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef REPORTING_CONTEXT_H_INCLUDED
#define REPORTING_CONTEXT_H_INCLUDED

#include <boost/json.hpp>

#include <optional>

#include <backend/BackendInterface.h>

class WsBase;
class SubscriptionManager;
class ETLLoadBalancer;

namespace RPC
{

struct Context
{
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    std::shared_ptr<BackendInterface> const& backend;
    std::shared_ptr<SubscriptionManager> const& subscriptions;
    std::shared_ptr<ETLLoadBalancer> const& balancer;
    std::shared_ptr<WsBase> session;
    Backend::LedgerRange const& range;

    Context(
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        std::shared_ptr<BackendInterface> const& backend_,
        std::shared_ptr<SubscriptionManager> const& subscriptions_,
        std::shared_ptr<ETLLoadBalancer> const& balancer_,
        std::shared_ptr<WsBase> const& session_,
        Backend::LedgerRange const& range_)
    : method(command_)
    , version(version_)
    , params(params_)
    , backend(backend_)
    , subscriptions(subscriptions_)
    , balancer(balancer_)
    , session(session_)
    , range(range_)
    {}
};


std::optional<Context>
make_WsContext(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<WsBase> const& session,
    Backend::LedgerRange const& range);

std::optional<Context>
make_HttpContext(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    Backend::LedgerRange const& range);
    
} // namespace RPC

#endif //REPORTING_CONTEXT_H_INCLUDED