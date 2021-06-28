
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


#ifndef REPORTING_SUBSCRIBE_HANDLER_H_INCLUDED
#define REPORTING_SUBSCRIBE_HANDLER_H_INCLUDED

#include <rpc/Context.h>
#include <rpc/Status.h>
#include <rpc/Handlers.h>
#include <boost/json.hpp>

namespace RPC
{
    
class Subscribe
{
public:
    explicit Subscribe(
        Context& ctx,
        boost::json::object& response)
    : context_(ctx)
    , response_(response) {}

    Status
    check();

    static char const*
    name()
    {
        return "subscribe";
    }

    static Role
    role()
    {
        return Role::USER;
    }

private:
    Context& context_;
    boost::json::object& response_;
};

class Unsubscribe
{
public:
    explicit Unsubscribe(
        Context& ctx,
        boost::json::object& response)
    : context_(ctx)
    , response_(response) {}

    Status
    check();

    static char const*
    name()
    {
        return "unsubscribe";
    }

    static Role
    role()
    {
        return Role::USER;
    }

private:
    Context& context_;
    boost::json::object& response_;
};

} // namespace RPC

#endif // REPORTING_SUBSCRIBE_HANDLER_H_INCLUDED