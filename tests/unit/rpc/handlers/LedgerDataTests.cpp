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

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kRANGE_MIN = 10;
constexpr auto kRANGE_MAX = 30;
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTXN_ID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F0DD";

}  // namespace

struct RPCLedgerDataHandlerTest : HandlerBaseTest {
    RPCLedgerDataHandlerTest()
    {
        backend_->setRange(kRANGE_MIN, kRANGE_MAX);
    }
};

struct LedgerDataParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerDataParameterTest : public RPCLedgerDataHandlerTest,
                                 public WithParamInterface<LedgerDataParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<LedgerDataParamTestCaseBundle>{
        LedgerDataParamTestCaseBundle{
            .testName = "ledger_indexInvalid",
            .testJson = R"({"ledger_index": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "ledger_hashInvalid",
            .testJson = R"({"ledger_hash": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "ledger_hashNotString",
            .testJson = R"({"ledger_hash": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "binaryNotBool",
            .testJson = R"({"binary": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitNotInt",
            .testJson = R"({"limit": "xxx"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitNagetive",
            .testJson = R"({"limit": -1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitZero",
            .testJson = R"({"limit": 0})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerInvalid",
            .testJson = R"({"marker": "xxx"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerOutOfOrder",
            .testJson = R"({
                "marker": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "out_of_order": true
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "outOfOrderMarkerNotInt"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerNotString",
            .testJson = R"({"marker": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerNotString"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "typeNotString",
            .testJson = R"({"type": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type', not string."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "typeNotValid",
            .testJson = R"({"type": "xxx"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type'."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerDataGroup1,
    LedgerDataParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(LedgerDataParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": {}
            }})",
            kRANGE_MAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": "{}"
            }})",
            kRANGE_MAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_hash": "{}"
            }})",
            kLEDGER_HASH
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerDataHandlerTest, MarkerNotExist)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kINDEX1}, kRANGE_MAX, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "marker": "{}"
            }})",
            kINDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerDoesNotExist");
    });
}

TEST_F(RPCLedgerDataHandlerTest, NoMarker)
{
    static auto const kLEDGER_EXPECTED = R"({
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "close_time_iso":"2000-01-01T00:00:00Z",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    // when 'type' not specified, default to all the types
    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limitLine--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kACCOUNT, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({"limit":10})");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per platform. It is however
        // guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), json::parse(kLEDGER_EXPECTED));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, Version2)
{
    static auto const kLEDGER_EXPECTED = R"({
      "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags": 0,
      "close_time": 0,
      "close_time_resolution": 0,
      "close_time_iso": "2000-01-01T00:00:00Z",
      "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index": 30,
      "parent_close_time": 0,
      "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins": "0",
      "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
      "closed": true
   })";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    // When 'type' not specified, default to all the types
    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limitLine--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kACCOUNT, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({"limit":10})");
        auto output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per platform. It is however
        // guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), json::parse(kLEDGER_EXPECTED));
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilter)
{
    static auto const kLEDGER_EXPECTED = R"({
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "close_time_iso":"2000-01-01T00:00:00Z",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limitLine--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kACCOUNT, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({
            "limit":10,
            "type":"state"
        })");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per platform. It is however
        // guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), json::parse(kLEDGER_EXPECTED));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 5);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterAMM)
{
    static auto const kLEDGER_EXPECTED = R"({
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "close_time_iso":"2000-01-01T00:00:00Z",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limitLine = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + 1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limitLine--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    auto const amm = createAmmObject(kACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kACCOUNT2);
    bbs.push_back(amm.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({
            "limit":6,
            "type":"amm"
        })");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per platform. It is however
        // guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), json::parse(kLEDGER_EXPECTED));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, OutOfOrder)
{
    static auto const kLEDGER_EXPECTED = R"({
      "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "close_flags":0,
      "close_time":0,
      "close_time_resolution":0,
      "close_time_iso":"2000-01-01T00:00:00Z",
      "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
      "ledger_index":"30",
      "parent_close_time":0,
      "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "total_coins":"0",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
      "closed":true
   })";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    // page end
    // marker return seq
    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(2);
    ON_CALL(*backend_, doFetchSuccessorKey(kFIRST_KEY, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));
    ON_CALL(*backend_, doFetchSuccessorKey(ripple::uint256{kINDEX2}, kRANGE_MAX, _))
        .WillByDefault(Return(std::nullopt));

    auto const line = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
    bbs.push_back(line.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({"limit":10, "out_of_order":true})");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), json::parse(kLEDGER_EXPECTED));
        EXPECT_EQ(output.result->as_object().at("marker").as_uint64(), kRANGE_MAX);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, Marker)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kINDEX1}, kRANGE_MAX, _))
        .WillByDefault(
            Return(createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123)
                       .getSerializer()
                       .peekData())
        );

    auto limit = 10;
    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limit);
    ON_CALL(*backend_, doFetchSuccessorKey(ripple::uint256{kINDEX1}, kRANGE_MAX, _))
        .WillByDefault(Return(ripple::uint256{kINDEX2}));
    ON_CALL(*backend_, doFetchSuccessorKey(ripple::uint256{kINDEX2}, kRANGE_MAX, _))
        .WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limit--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":10,
                "marker": "{}"
            }})",
            kINDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, DiffMarker)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limit = 10;
    std::vector<LedgerObject> los;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, fetchLedgerDiff).Times(1);

    while ((limit--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
        los.emplace_back(LedgerObject{.key = ripple::uint256{kINDEX2}, .blob = Blob{}}
        );  // NOLINT(modernize-use-emplace)
    }
    ON_CALL(*backend_, fetchLedgerDiff(kRANGE_MAX, _)).WillByDefault(Return(los));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":10,
                "marker": {},
                "out_of_order": true
            }})",
            kRANGE_MAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
        EXPECT_FALSE(output.result->as_object().at("cache_full").as_bool());
    });
}

TEST_F(RPCLedgerDataHandlerTest, Binary)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limit = 10;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limit);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limit--) != 0) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(
            R"({
                "limit":10,
                "binary": true
            })"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, BinaryLimitMoreThanMax)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limit = LedgerDataHandler::kLIMIT_BINARY + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(LedgerDataHandler::kLIMIT_BINARY);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limit--) != 0u) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":{},
                "binary": true
            }})",
            LedgerDataHandler::kLIMIT_BINARY + 1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), LedgerDataHandler::kLIMIT_BINARY);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, JsonLimitMoreThanMax)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    auto limit = LedgerDataHandler::kLIMIT_JSON + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(LedgerDataHandler::kLIMIT_JSON);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    while ((limit--) != 0u) {
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "limit":{},
                "binary": false
            }})",
            LedgerDataHandler::kLIMIT_JSON + 1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), LedgerDataHandler::kLIMIT_JSON);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterMPTIssuance)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    auto const issuance = createMptIssuanceObject(kACCOUNT, 2, "metadata");
    bbs.push_back(issuance.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({
            "limit":1,
            "type":"mpt_issuance"
        })");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);

        auto const& objects = output.result->as_object().at("state").as_array();
        EXPECT_EQ(objects.front().at("LedgerEntryType").as_string(), "MPTokenIssuance");

        // make sure mptID is synethetically parsed if object is mptIssuance
        EXPECT_EQ(
            objects.front().at("mpt_issuance_id").as_string(),
            ripple::to_string(ripple::makeMptID(2, getAccountIdWithString(kACCOUNT)))
        );
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterMPToken)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillByDefault(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRANGE_MAX, _)).WillByDefault(Return(ripple::uint256{kINDEX2}));

    auto const mptoken = createMpTokenObject(kACCOUNT, ripple::makeMptID(2, getAccountIdWithString(kACCOUNT)));
    bbs.push_back(mptoken.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = json::parse(R"({
            "limit":1,
            "type":"mptoken"
        })");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kINDEX2);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLEDGER_HASH);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRANGE_MAX);

        auto const& objects = output.result->as_object().at("state").as_array();
        EXPECT_EQ(objects.front().at("LedgerEntryType").as_string(), "MPToken");
    });
}

TEST(RPCLedgerDataHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"ledger", 1},
        {"out_of_order", true},
        {"ledger_hash", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"ledger_index", 1},
        {"limit", 10},
        {"marker", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"type", "state"},
        {"ledger", "some"}
    };
    auto const spec = LedgerDataHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated));
    EXPECT_NE(warning.at("message").as_string().find("Field 'ledger' is deprecated."), std::string::npos) << warning;
}
