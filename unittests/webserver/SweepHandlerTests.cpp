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

#include <rpc/handlers/impl/FakesAndMocks.h>
#include <util/Fixtures.h>
#include <util/config/Config.h>
#include <webserver/DOSGuard.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>

using namespace util;
using namespace web;
using namespace testing;

constexpr static auto JSONData = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 0.1,
            "max_connections": 2,
            "whitelist": ["127.0.0.1"]
        }
    }
)JSON";

class DOSGuardIntervalSweepHandlerTest : public SyncAsioContextTest
{
protected:
    Config cfg{boost::json::parse(JSONData)};
    IntervalSweepHandler sweepHandler{cfg, ctx};
    unittests::detail::BasicDOSGuardMock<IntervalSweepHandler> guard{sweepHandler};
};

TEST_F(DOSGuardIntervalSweepHandlerTest, SweepAfterInterval)
{
    EXPECT_CALL(guard, clear()).Times(AtLeast(2));
    ctx.run_for(std::chrono::milliseconds(400));
}