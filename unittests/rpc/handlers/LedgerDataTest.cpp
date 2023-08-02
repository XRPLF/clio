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
#include <rpc/handlers/LedgerData.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F0DD";

class RPCLedgerDataHandlerTest : public HandlerBaseTest
{
};

struct LedgerDataParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerDataParameterTest : public RPCLedgerDataHandlerTest,
                                 public WithParamInterface<LedgerDataParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<LedgerDataParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<LedgerDataParamTestCaseBundle>{
        LedgerDataParamTestCaseBundle{
            "ledger_indexInvalid", R"({"ledger_index": "x"})", "invalidParams", "ledgerIndexMalformed"},
        LedgerDataParamTestCaseBundle{
            "ledger_hashInvalid", R"({"ledger_hash": "x"})", "invalidParams", "ledger_hashMalformed"},
        LedgerDataParamTestCaseBundle{
            "ledger_hashNotString", R"({"ledger_hash": 123})", "invalidParams", "ledger_hashNotString"},
        LedgerDataParamTestCaseBundle{"binaryNotBool", R"({"binary": 123})", "invalidParams", "Invalid parameters."},
        LedgerDataParamTestCaseBundle{"limitNotInt", R"({"limit": "xxx"})", "invalidParams", "Invalid parameters."},
        LedgerDataParamTestCaseBundle{"limitNagetive", R"({"limit": -1})", "invalidParams", "Invalid parameters."},
        LedgerDataParamTestCaseBundle{"limitZero", R"({"limit": 0})", "invalidParams", "Invalid parameters."},
        LedgerDataParamTestCaseBundle{"markerInvalid", R"({"marker": "xxx"})", "invalidParams", "markerMalformed"},
        LedgerDataParamTestCaseBundle{
            "markerOutOfOrder",
            R"({
                "marker": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "out_of_order": true
            })",
            "invalidParams",
            "outOfOrderMarkerNotInt"},
        LedgerDataParamTestCaseBundle{"markerNotString", R"({"marker": 123})", "invalidParams", "markerNotString"},
        LedgerDataParamTestCaseBundle{
            "typeNotString", R"({"type": 123})", "invalidParams", "Invalid field 'type', not string."},
        LedgerDataParamTestCaseBundle{"typeNotValid", R"({"type": "xxx"})", "invalidParams", "Invalid field 'type'."},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerDataGroup1,
    LedgerDataParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    LedgerDataParameterTest::NameGenerator{});

TEST_P(LedgerDataParameterTest, InvalidParams)
{
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": {}
            }})",
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": "{}"
            }})",
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_hash": "{}"
            }})",
            LEDGERHASH));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, MarkerNotExist)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "marker": "{}"
            }})",
            INDEX1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerDoesNotExist");
    });
}

TEST_F(RPCLedgerDataHandlerTest, NoMarker)
{
    static auto const ledgerExpected = R"({
      "accepted":true,
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    // when 'type' not specified, default to all the types
    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limitLine--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    while (limitTicket--)
    {
        auto const ticket = CreateTicketLedgerObject(ACCOUNT, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(R"({"limit":10})");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        //"close_time_human" 's format depends on platform, might be sightly different
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output->as_object().at("ledger"), json::parse(ledgerExpected));
        EXPECT_EQ(output->as_object().at("marker").as_string(), INDEX2);
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilter)
{
    static auto const ledgerExpected = R"({
      "accepted":true,
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limitLine--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    while (limitTicket--)
    {
        auto const ticket = CreateTicketLedgerObject(ACCOUNT, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(R"({
            "limit":10,
            "type":"state"
        })");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        //"close_time_human" 's format depends on platform, might be sightly different
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output->as_object().at("ledger"), json::parse(ledgerExpected));
        EXPECT_EQ(output->as_object().at("marker").as_string(), INDEX2);
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 5);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, OutOfOrder)
{
    static auto const ledgerExpected = R"({
      "accepted":true,
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    // page end
    // marker return seq
    std::vector<Blob> bbs;
    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(2);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(firstKey, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(ripple::uint256{INDEX2}, RANGEMAX, _))
        .WillByDefault(Return(std::nullopt));

    auto const line =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
    bbs.push_back(line.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(R"({"limit":10, "out_of_order":true})");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output->as_object().at("ledger"), json::parse(ledgerExpected));
        EXPECT_EQ(output->as_object().at("marker").as_uint64(), RANGEMAX);
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, Marker)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillByDefault(
            Return(CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123)
                       .getSerializer()
                       .peekData()));

    auto limit = 10;
    std::vector<Blob> bbs;
    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(limit);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillByDefault(Return(ripple::uint256{INDEX2}));
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(ripple::uint256{INDEX2}, RANGEMAX, _))
        .WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limit--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":10,
                "marker": "{}"
            }})",
            INDEX1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output->as_object().contains("ledger"));
        EXPECT_EQ(output->as_object().at("marker").as_string(), INDEX2);
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, DiffMarker)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    auto limit = 10;
    std::vector<LedgerObject> los;
    std::vector<Blob> bbs;

    EXPECT_CALL(*rawBackendPtr, fetchLedgerDiff).Times(1);

    while (limit--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
        los.push_back(LedgerObject{ripple::uint256{INDEX2}, Blob{}});
    }
    ON_CALL(*rawBackendPtr, fetchLedgerDiff(RANGEMAX, _)).WillByDefault(Return(los));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":10,
                "marker": {},
                "out_of_order": true
            }})",
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output->as_object().contains("ledger"));
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
        EXPECT_FALSE(output->as_object().at("cache_full").as_bool());
    });
}

TEST_F(RPCLedgerDataHandlerTest, Binary)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    auto limit = 10;
    std::vector<Blob> bbs;

    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(limit);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limit--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "limit":10,
                "binary": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        EXPECT_TRUE(output->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, BinaryLimitMoreThanMax)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    auto limit = LedgerDataHandler::LIMITBINARY + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(LedgerDataHandler::LIMITBINARY);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limit--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":{},
                "binary": true
            }})",
            LedgerDataHandler::LIMITBINARY + 1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        EXPECT_TRUE(output->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output->as_object().at("state").as_array().size(), LedgerDataHandler::LIMITBINARY);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, JsonLimitMoreThanMax)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _))
        .WillByDefault(Return(CreateLedgerInfo(LEDGERHASH, RANGEMAX)));

    auto limit = LedgerDataHandler::LIMITJSON + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(LedgerDataHandler::LIMITJSON);
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, RANGEMAX, _)).WillByDefault(Return(ripple::uint256{INDEX2}));

    while (limit--)
    {
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":{},
                "binary": false
            }})",
            LedgerDataHandler::LIMITJSON + 1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
        EXPECT_TRUE(output->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output->as_object().at("state").as_array().size(), LedgerDataHandler::LIMITJSON);
        EXPECT_EQ(output->as_object().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output->as_object().at("ledger_index").as_uint64(), RANGEMAX);
    });
}
