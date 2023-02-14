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

#include <rpc/common/AnyHandler.h>
#include <rpc/ngHandlers/AccountChannels.h>
#include <util/Fixtures.h>

using namespace RPCng;
namespace json = boost::json;

class RPCAccountHandlerTest : public SyncAsioContextTest
{
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
        clio::Config cfg;
        mockBackendPtr = std::make_shared<MockBackend>(cfg);
    }
    void
    TearDown() override
    {
        mockBackendPtr.reset();
    }

protected:
    std::shared_ptr<BackendInterface> mockBackendPtr;
};

TEST_F(RPCAccountHandlerTest, NonHexLedgerHash)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto const handler =
            AnyHandler{AccountChannelsHandler{yield, mockBackendPtr}};
        auto const input = json::parse(R"({ 
        "account": "myaccount", 
        "limit": 10,
        "ledger_hash": "xxx"
        })");
        auto const output = handler.process(input);
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerHashMalformed");
    });
    ctx.run();
}

TEST_F(RPCAccountHandlerTest, NonStringLedgerHash)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto const handler =
            AnyHandler{AccountChannelsHandler{yield, mockBackendPtr}};
        auto const input = json::parse(R"({ 
        "account": "myaccount", 
        "limit": 10,
        "ledger_hash": 123
        })");
        auto const output = handler.process(input);
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        std::cout << err.at("error_message").as_string() << std::endl;
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerHashNotString");
    });
    ctx.run();
}
