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

#include <rpc/common/impl/AdminVerificationStrategy.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

class RPCAdminVerificationTest : public NoLoggerFixture
{
protected:
    RPC::detail::IPAdminVerificationStrategy strat_;
};

TEST_F(RPCAdminVerificationTest, IsAdminOnlyForIP_127_0_0_1)
{
    EXPECT_TRUE(strat_.isAdmin("127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin("127.0.0.2"));
    EXPECT_FALSE(strat_.isAdmin("127"));
    EXPECT_FALSE(strat_.isAdmin(""));
    EXPECT_FALSE(strat_.isAdmin("localhost"));
}
