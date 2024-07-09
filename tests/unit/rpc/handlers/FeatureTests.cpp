//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Feature.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace rpc;

class RPCFeatureHandlerTest : public HandlerBaseTest {
protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr;
};

TEST_F(RPCFeatureHandlerTest, AlwaysNoPermissionForVetoed)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output =
            handler.process(boost::json::parse(R"({"vetoed": true, "feature": "foo"})"), Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "noPermission");
        EXPECT_EQ(
            err.at("error_message").as_string(), "The admin portion of feature API is not available through Clio."
        );
    });
}
