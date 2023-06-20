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

#include <util/Fixtures.h>
#include <util/MockCounters.h>
#include <util/MockHandlerProvider.h>
#include <util/MockLoadBalancer.h>

#include <rpc/common/impl/ForwardingProxy.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

using namespace clio;
using namespace RPC;

class RPCForwardingProxyTest : public NoLoggerFixture
{
protected:
    MockLoadBalancer loadBalancer;
    MockCounters counters;
    MockHandlerProvider handlerProvider;

    RPC::detail::ForwardingProxy<MockCounters> proxy{loadBalancer, counters, handlerProvider};
};

TEST_F(RPCForwardingProxyTest, Blah)
{
}
