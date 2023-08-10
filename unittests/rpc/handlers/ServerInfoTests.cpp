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
#include <rpc/handlers/ServerInfo.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

using TestServerInfoHandler =
    BaseServerInfoHandler<MockSubscriptionManager, MockLoadBalancer, MockETLService, MockCounters>;

constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto CLIENTIP = "1.1.1.1";

class RPCServerInfoHandlerTest : public HandlerBaseTest,
                                 public MockLoadBalancerTest,
                                 public MockSubscriptionManagerTest,
                                 public MockETLServiceTest,
                                 public MockCountersTest
{
protected:
    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        MockLoadBalancerTest::SetUp();
        MockSubscriptionManagerTest::SetUp();
        MockETLServiceTest::SetUp();
        MockCountersTest::SetUp();
    }

    void
    TearDown() override
    {
        MockCountersTest::TearDown();
        MockETLServiceTest::TearDown();
        MockSubscriptionManagerTest::TearDown();
        MockLoadBalancerTest::TearDown();
        HandlerBaseTest::TearDown();
    }

    void
    validateNormalOutput(RPC::ReturnType const& output)
    {
        ASSERT_TRUE(output);
        auto const& result = output.value().as_object();
        EXPECT_TRUE(result.contains("info"));

        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("complete_ledgers"));
        EXPECT_STREQ(info.at("complete_ledgers").as_string().c_str(), "10-30");
        EXPECT_TRUE(info.contains("load_factor"));
        EXPECT_TRUE(info.contains("clio_version"));
        EXPECT_TRUE(info.contains("validated_ledger"));
        EXPECT_TRUE(info.contains("time"));
        EXPECT_TRUE(info.contains("uptime"));

        auto const& validated = info.at("validated_ledger").as_object();
        EXPECT_TRUE(validated.contains("age"));
        EXPECT_EQ(validated.at("age").as_uint64(), 3u);
        EXPECT_TRUE(validated.contains("hash"));
        EXPECT_STREQ(validated.at("hash").as_string().c_str(), LEDGERHASH);
        EXPECT_TRUE(validated.contains("seq"));
        EXPECT_EQ(validated.at("seq").as_uint64(), 30u);
        EXPECT_TRUE(validated.contains("base_fee_xrp"));
        EXPECT_EQ(validated.at("base_fee_xrp").as_double(), 1e-06);
        EXPECT_TRUE(validated.contains("reserve_base_xrp"));
        EXPECT_EQ(validated.at("reserve_base_xrp").as_double(), 3e-06);
        EXPECT_TRUE(validated.contains("reserve_inc_xrp"));
        EXPECT_EQ(validated.at("reserve_inc_xrp").as_double(), 2e-06);

        auto const& cache = info.at("cache").as_object();
        EXPECT_TRUE(cache.contains("size"));
        EXPECT_TRUE(cache.contains("is_full"));
        EXPECT_TRUE(cache.contains("latest_ledger_seq"));
        EXPECT_TRUE(cache.contains("object_hit_rate"));
        EXPECT_TRUE(cache.contains("successor_hit_rate"));
    }

    void
    validateAdminOutput(RPC::ReturnType const& output)
    {
        auto const& result = output.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("etl"));
        EXPECT_TRUE(info.contains("counters"));
    }

    void
    validateRippledOutput(RPC::ReturnType const& output)
    {
        auto const& result = output.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("load_factor"));
        EXPECT_EQ(info.at("load_factor").as_int64(), 234);
        EXPECT_TRUE(info.contains("validation_quorum"));
        EXPECT_EQ(info.at("validation_quorum").as_int64(), 456);
        EXPECT_TRUE(info.contains("rippled_version"));
        EXPECT_STREQ(info.at("rippled_version").as_string().c_str(), "1234");
        EXPECT_TRUE(info.contains("network_id"));
        EXPECT_EQ(info.at("network_id").as_int64(), 2);
    }

    void
    validateCacheOutput(RPC::ReturnType const& output)
    {
        auto const& result = output.value().as_object();
        auto const& info = result.at("info").as_object();
        auto const& cache = info.at("cache").as_object();
        EXPECT_EQ(cache.at("size").as_uint64(), 1u);
        EXPECT_EQ(cache.at("is_full").as_bool(), false);
        EXPECT_EQ(cache.at("latest_ledger_seq").as_uint64(), 30u);
        EXPECT_EQ(cache.at("object_hit_rate").as_double(), 1.0);
        EXPECT_EQ(cache.at("successor_hit_rate").as_double(), 1.0);
    }
};

TEST_F(RPCServerInfoHandlerTest, NoLedgerInfoErrorsOutWithInternal)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "internal");
        EXPECT_EQ(err.at("error_message").as_string(), "Internal error.");
    });
}

TEST_F(RPCServerInfoHandlerTest, NoFeesErrorsOutWithInternal)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "internal");
        EXPECT_EQ(err.at("error_message").as_string(), "Internal error.");
    });
}

TEST_F(RPCServerInfoHandlerTest, DefaultOutputIsPresent)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    MockLoadBalancer* rawBalancerPtr = static_cast<MockLoadBalancer*>(mockLoadBalancerPtr.get());
    MockCounters* rawCountersPtr = static_cast<MockCounters*>(mockCountersPtr.get());
    MockETLService* rawETLServicePtr = static_cast<MockETLService*>(mockETLServicePtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30, 3);  // 3 seconds old
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), testing::_)).Times(1);

    ON_CALL(*rawCountersPtr, uptime).WillByDefault(Return(std::chrono::seconds{1234}));
    EXPECT_CALL(*rawCountersPtr, uptime).Times(1);

    ON_CALL(*rawETLServicePtr, isAmendmentBlocked).WillByDefault(Return(false));
    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        // no admin section present by default
        auto const& result = output.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_FALSE(info.contains("etl"));
        EXPECT_FALSE(info.contains("counters"));
    });
}

TEST_F(RPCServerInfoHandlerTest, AmendmentBlockedIsPresentIfSet)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    MockLoadBalancer* rawBalancerPtr = static_cast<MockLoadBalancer*>(mockLoadBalancerPtr.get());
    MockCounters* rawCountersPtr = static_cast<MockCounters*>(mockCountersPtr.get());
    MockETLService* rawETLServicePtr = static_cast<MockETLService*>(mockETLServicePtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30, 3);  // 3 seconds old
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), testing::_)).Times(1);

    ON_CALL(*rawCountersPtr, uptime).WillByDefault(Return(std::chrono::seconds{1234}));
    EXPECT_CALL(*rawCountersPtr, uptime).Times(1);

    ON_CALL(*rawETLServicePtr, isAmendmentBlocked).WillByDefault(Return(true));
    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        auto const& info = output.value().as_object().at("info").as_object();
        EXPECT_TRUE(info.contains("amendment_blocked"));
        EXPECT_EQ(info.at("amendment_blocked").as_bool(), true);
    });
}

TEST_F(RPCServerInfoHandlerTest, AdminSectionPresentWhenAdminFlagIsSet)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    MockLoadBalancer* rawBalancerPtr = static_cast<MockLoadBalancer*>(mockLoadBalancerPtr.get());
    MockCounters* rawCountersPtr = static_cast<MockCounters*>(mockCountersPtr.get());
    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    MockETLService* rawETLServicePtr = static_cast<MockETLService*>(mockETLServicePtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const empty = boost::json::object{};
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30, 3);  // 3 seconds old
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(empty));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).Times(1);

    ON_CALL(*rawCountersPtr, uptime).WillByDefault(Return(std::chrono::seconds{1234}));
    EXPECT_CALL(*rawCountersPtr, uptime).Times(1);

    ON_CALL(*rawETLServicePtr, isAmendmentBlocked).WillByDefault(Return(false));
    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).Times(1);

    // admin calls
    ON_CALL(*rawCountersPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawCountersPtr, report).Times(1);

    ON_CALL(*rawSubscriptionManagerPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawSubscriptionManagerPtr, report).Times(1);

    ON_CALL(*rawETLServicePtr, getInfo).WillByDefault(Return(empty));
    EXPECT_CALL(*rawETLServicePtr, getInfo).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output);
    });
}

TEST_F(RPCServerInfoHandlerTest, RippledForwardedValuesPresent)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    MockLoadBalancer* rawBalancerPtr = static_cast<MockLoadBalancer*>(mockLoadBalancerPtr.get());
    MockCounters* rawCountersPtr = static_cast<MockCounters*>(mockCountersPtr.get());
    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    MockETLService* rawETLServicePtr = static_cast<MockETLService*>(mockETLServicePtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const empty = boost::json::object{};
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30, 3);  // 3 seconds old
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    ON_CALL(*rawCountersPtr, uptime).WillByDefault(Return(std::chrono::seconds{1234}));
    EXPECT_CALL(*rawCountersPtr, uptime).Times(1);

    ON_CALL(*rawETLServicePtr, isAmendmentBlocked).WillByDefault(Return(false));
    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).Times(1);

    auto const rippledObj = boost::json::parse(R"({
        "result": {
            "info": {
                "build_version": "1234",
                "validation_quorum": 456,
                "load_factor": 234,
                "network_id": 2
            }
        }
    })");
    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(rippledObj.as_object()));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).Times(1);

    // admin calls
    ON_CALL(*rawCountersPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawCountersPtr, report).Times(1);

    ON_CALL(*rawSubscriptionManagerPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawSubscriptionManagerPtr, report).Times(1);

    ON_CALL(*rawETLServicePtr, getInfo).WillByDefault(Return(empty));
    EXPECT_CALL(*rawETLServicePtr, getInfo).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output);
        validateRippledOutput(output);
    });
}

TEST_F(RPCServerInfoHandlerTest, RippledForwardedValuesMissingNoExceptionThrown)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    MockLoadBalancer* rawBalancerPtr = static_cast<MockLoadBalancer*>(mockLoadBalancerPtr.get());
    MockCounters* rawCountersPtr = static_cast<MockCounters*>(mockCountersPtr.get());
    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    MockETLService* rawETLServicePtr = static_cast<MockETLService*>(mockETLServicePtr.get());

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto const empty = boost::json::object{};
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30, 3);  // 3 seconds old
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    auto const feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    ON_CALL(*rawCountersPtr, uptime).WillByDefault(Return(std::chrono::seconds{1234}));
    EXPECT_CALL(*rawCountersPtr, uptime).Times(1);

    ON_CALL(*rawETLServicePtr, isAmendmentBlocked).WillByDefault(Return(false));
    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).Times(1);

    auto const rippledObj = boost::json::parse(R"({
        "result": {
            "info": {}
        }
    })");
    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(rippledObj.as_object()));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).Times(1);

    // admin calls
    ON_CALL(*rawCountersPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawCountersPtr, report).Times(1);

    ON_CALL(*rawSubscriptionManagerPtr, report).WillByDefault(Return(empty));
    EXPECT_CALL(*rawSubscriptionManagerPtr, report).Times(1);

    ON_CALL(*rawETLServicePtr, getInfo).WillByDefault(Return(empty));
    EXPECT_CALL(*rawETLServicePtr, getInfo).Times(1);

    auto const handler = AnyHandler{TestServerInfoHandler{
        mockBackendPtr, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr}};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output);
    });
}

// TODO: In the future we'd like to refactor to add mock and test for cache
