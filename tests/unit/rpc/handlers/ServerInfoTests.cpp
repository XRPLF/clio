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

#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/ServerInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockCounters.hpp"
#include "util/MockCountersFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestObject.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_to.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

using TestServerInfoHandler = BaseServerInfoHandler<MockLoadBalancer, MockETLService, MockCounters>;

constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto CLIENTIP = "1.1.1.1";

struct RPCServerInfoHandlerTest : HandlerBaseTest, MockLoadBalancerTest, MockCountersTest {
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;

    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        MockLoadBalancerTest::SetUp();
        MockCountersTest::SetUp();

        backend->setRange(10, 30);
    }

    void
    TearDown() override
    {
        MockCountersTest::TearDown();
        MockLoadBalancerTest::TearDown();
        HandlerBaseTest::TearDown();
    }

    static void
    validateNormalOutput(rpc::ReturnType const& output)
    {
        ASSERT_TRUE(output);
        auto const& result = output.result.value().as_object();
        EXPECT_TRUE(result.contains("info"));

        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("complete_ledgers"));
        EXPECT_EQ(boost::json::value_to<std::string>(info.at("complete_ledgers")), "10-30");
        EXPECT_TRUE(info.contains("load_factor"));
        EXPECT_TRUE(info.contains("clio_version"));
        EXPECT_TRUE(info.contains("libxrpl_version"));
        EXPECT_TRUE(info.contains("validated_ledger"));
        EXPECT_TRUE(info.contains("time"));
        EXPECT_TRUE(info.contains("uptime"));

        auto const& validated = info.at("validated_ledger").as_object();
        EXPECT_TRUE(validated.contains("age"));
        EXPECT_EQ(validated.at("age").as_uint64(), 3u);
        EXPECT_TRUE(validated.contains("hash"));
        EXPECT_EQ(boost::json::value_to<std::string>(validated.at("hash")), LEDGERHASH);
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
        EXPECT_TRUE(cache.contains("is_enabled"));
    }

    static void
    validateAdminOutput(rpc::ReturnType const& output, bool shouldHaveBackendCounters = false)
    {
        auto const& result = output.result.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("etl"));
        EXPECT_TRUE(info.contains("counters"));
        if (shouldHaveBackendCounters) {
            ASSERT_TRUE(info.contains("backend_counters")) << boost::json::serialize(info);
            EXPECT_TRUE(info.at("backend_counters").is_object());
            EXPECT_TRUE(!info.at("backend_counters").as_object().empty());
        }
    }

    static void
    validateRippledOutput(rpc::ReturnType const& output)
    {
        auto const& result = output.result.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_TRUE(info.contains("load_factor"));
        EXPECT_EQ(info.at("load_factor").as_int64(), 234);
        EXPECT_TRUE(info.contains("validation_quorum"));
        EXPECT_EQ(info.at("validation_quorum").as_int64(), 456);
        EXPECT_TRUE(info.contains("rippled_version"));
        EXPECT_EQ(boost::json::value_to<std::string>(info.at("rippled_version")), "1234");
        EXPECT_TRUE(info.contains("network_id"));
        EXPECT_EQ(info.at("network_id").as_int64(), 2);
    }
};

TEST_F(RPCServerInfoHandlerTest, NoLedgerHeaderErrorsOutWithInternal)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(std::nullopt));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "internal");
        EXPECT_EQ(err.at("error_message").as_string(), "Internal error.");
    });
}

TEST_F(RPCServerInfoHandlerTest, NoFeesErrorsOutWithInternal)
{
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(std::nullopt));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "internal");
        EXPECT_EQ(err.at("error_message").as_string(), "Internal error.");
    });
}

TEST_F(RPCServerInfoHandlerTest, DefaultOutputIsPresent)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), false, testing::_))
        .WillOnce(Return(std::nullopt));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(false));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        // no admin section present by default
        auto const& result = output.result.value().as_object();
        auto const& info = result.at("info").as_object();
        EXPECT_FALSE(info.contains("etl"));
        EXPECT_FALSE(info.contains("counters"));
    });
}

TEST_F(RPCServerInfoHandlerTest, AmendmentBlockedIsPresentIfSet)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), false, testing::_))
        .WillOnce(Return(std::nullopt));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(true));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        auto const& info = output.result.value().as_object().at("info").as_object();
        EXPECT_TRUE(info.contains("amendment_blocked"));
        EXPECT_EQ(info.at("amendment_blocked").as_bool(), true);
    });
}

TEST_F(RPCServerInfoHandlerTest, CorruptionDetectedIsPresentIfSet)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), false, testing::_))
        .WillOnce(Return(std::nullopt));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isCorruptionDetected).WillOnce(Return(true));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        auto const& info = output.result.value().as_object().at("info").as_object();
        EXPECT_TRUE(info.contains("corruption_detected"));
        EXPECT_EQ(info.at("corruption_detected").as_bool(), true);
    });
}

TEST_F(RPCServerInfoHandlerTest, CacheReportsEnabledFlagCorrectly)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(2).WillRepeatedly(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2).WillRepeatedly(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(testing::_, testing::Eq(CLIENTIP), false, testing::_))
        .Times(2)
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_CALL(*rawCountersPtr, uptime).Times(2).WillRepeatedly(Return(std::chrono::seconds{1234}));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        auto const& cache = output.result.value().as_object().at("info").as_object().at("cache").as_object();
        EXPECT_TRUE(cache.contains("is_enabled"));
        EXPECT_EQ(cache.at("is_enabled").as_bool(), true);
    });

    backend->cache().setDisabled();

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, false, CLIENTIP});

        validateNormalOutput(output);

        auto const& cache = output.result.value().as_object().at("info").as_object().at("cache").as_object();
        EXPECT_TRUE(cache.contains("is_enabled"));
        EXPECT_EQ(cache.at("is_enabled").as_bool(), false);
    });
}

TEST_F(RPCServerInfoHandlerTest, AdminSectionPresentWhenAdminFlagIsSet)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const empty = json::object{};
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).WillOnce(Return(empty));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(false));

    // admin calls
    EXPECT_CALL(*rawCountersPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*mockSubscriptionManagerPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*rawETLServicePtr, getInfo).WillOnce(Return(empty));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output);
    });
}

TEST_F(RPCServerInfoHandlerTest, BackendCountersPresentWhenRequestWithParam)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const empty = json::object{};
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).WillOnce(Return(empty));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(false));

    // admin calls
    EXPECT_CALL(*rawCountersPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*mockSubscriptionManagerPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*rawETLServicePtr, getInfo).WillOnce(Return(empty));

    EXPECT_CALL(*backend, stats).WillOnce(Return(boost::json::object{{"read_cout", 10}, {"write_count", 3}}));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse(R"(
        {
            "backend_counters": true
        }
        )");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output, true);
    });
}

TEST_F(RPCServerInfoHandlerTest, RippledForwardedValuesPresent)
{
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const empty = json::object{};
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(false));

    auto const rippledObj = json::parse(R"({
        "result": {
            "info": {
                "build_version": "1234",
                "validation_quorum": 456,
                "load_factor": 234,
                "network_id": 2
            }
        }
    })");
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).WillOnce(Return(rippledObj.as_object()));

    // admin calls
    EXPECT_CALL(*rawCountersPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*mockSubscriptionManagerPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*rawETLServicePtr, getInfo).WillOnce(Return(empty));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

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
    MockLoadBalancer* rawBalancerPtr = mockLoadBalancerPtr.get();
    MockCounters* rawCountersPtr = mockCountersPtr.get();
    MockETLService* rawETLServicePtr = mockETLServicePtr.get();

    auto const empty = json::object{};
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30, 3);  // 3 seconds old
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(feeBlob));

    EXPECT_CALL(*rawCountersPtr, uptime).WillOnce(Return(std::chrono::seconds{1234}));

    EXPECT_CALL(*rawETLServicePtr, isAmendmentBlocked).WillOnce(Return(false));

    auto const rippledObj = json::parse(R"({
        "result": {
            "info": {}
        }
    })");
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled).WillOnce(Return(rippledObj.as_object()));

    // admin calls
    EXPECT_CALL(*rawCountersPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*mockSubscriptionManagerPtr, report).WillOnce(Return(empty));

    EXPECT_CALL(*rawETLServicePtr, getInfo).WillOnce(Return(empty));

    auto const handler = AnyHandler{TestServerInfoHandler{
        backend, mockSubscriptionManagerPtr, mockLoadBalancerPtr, mockETLServicePtr, *mockCountersPtr
    }};

    runSpawn([&](auto yield) {
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield, {}, true});

        validateNormalOutput(output);
        validateAdminOutput(output);
    });
}

// TODO: In the future we'd like to refactor to add mock and test for cache
