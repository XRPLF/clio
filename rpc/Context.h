
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

struct RpcContext
{
    std::string const& method_;
    std::uint32_t version_;
    std::optional<boost::json::object> const& params_;
    std::shared_ptr<BackendInterface> const& backend_;
    std::shared_ptr<SubscriptionManager> const& subscriptions_;
    std::shared_ptr<ETLLoadBalancer> const& balancer_;

    RpcContext(
        std::string command,
        std::uint32_t version,
        std::optional<boost::json::object> const& params,
        std::shared_ptr<BackendInterface> const backend,
        std::shared_ptr<SubscriptionManager> const subscriptions,
        std::shared_ptr<ETLLoadBalancer> const balancer)
    : method_(command)
    , version_(version)
    , params_(params)
    , backend_(backend)
    , subscriptions_(subscriptions)
    , balancer_(balancer)
    {}
};

namespace RPC
{
    std::unique_ptr<RpcContext>
    make_WsContext(
        boost::json::object const& request,
        error_code& ec, 
        std::string& message)
    {
        if (!request.contains("command"))
            return nullptr;
        
        return std::make_unique<RpcContext>(

        )
    }

    std::unique_ptr<RpcContext>
    make_HttpContext(
        boost::json::object const& request,
        error_code& ec,
        std::string& message)
    {
        if (!req.contains("method") || !req.at("method").is_string())
            return nullptr;

        // empty params
        if (!req.contains("params"))
            return std::make_unique(
                req.at("method").as_string().c_str(),
                {},
                
            );

        if (!req.at("params").is_array())
            return nullptr;

        auto array = req.at("params").as_array();

        if (array.size() != 1)
            return nullptr;
        
        if (!array.at(0).is_object())
            return nullptr;
        
        return std::make_unique<RpcContext>(
            
        );
    }
    
} // namespace RPC

#endif //REPORTING_CONTEXT_H_INCLUDED