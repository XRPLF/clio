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
#include <fmt/format.h>
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
using namespace data;
using namespace testing;

namespace {

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTxnId = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F0DD";
constexpr auto kApiVersion = 2;

}  // namespace

struct RPCLedgerDataHandlerTest : HandlerBaseTest {
    RPCLedgerDataHandlerTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
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
            .testJson = R"JSON({"ledger_index": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "ledger_hashInvalid",
            .testJson = R"JSON({"ledger_hash": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "ledger_hashNotString",
            .testJson = R"JSON({"ledger_hash": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "binaryNotBool",
            .testJson = R"JSON({"binary": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitNotInt",
            .testJson = R"JSON({"limit": "xxx"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitNegative",
            .testJson = R"JSON({"limit": -1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "limitZero",
            .testJson = R"JSON({"limit": 0})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerInvalid",
            .testJson = R"JSON({"marker": "xxx"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerMalformed"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerOutOfOrder",
            .testJson = R"JSON({
                "marker": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "out_of_order": true
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "outOfOrderMarkerNotInt"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "markerNotString",
            .testJson = R"JSON({"marker": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerNotString"
        },
        LedgerDataParamTestCaseBundle{
            .testName = "typeNotString",
            .testJson = R"JSON({"type": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type', not string."
        },
        LedgerDataParamTestCaseBundle{
            .testName = "typeNotValid",
            .testJson = R"JSON({"type": "xxx"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type'."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerDataGroup1,
    LedgerDataParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(LedgerDataParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
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
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "ledger_index": {}
                }})JSON",
                kRangeMax
            )
        );
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
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "ledger_index": "{}"
                }})JSON",
                kRangeMax
            )
        );
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
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "ledger_hash": "{}"
                }})JSON",
                kLedgerHash
            )
        );
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
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "marker": "{}"
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerDoesNotExist");
    });
}

TEST_F(RPCLedgerDataHandlerTest, NoMarker)
{
    static auto const kLedgerExpected = R"JSON({
        "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "close_flags": 0,
        "close_time": 0,
        "close_time_resolution": 0,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": "30",
        "parent_close_time": 0,
        "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "total_coins": "0",
        "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "closed": true
    })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    // when 'type' not specified, default to all the types
    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limitLine--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kAccount, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({"limit": 10})JSON");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per
        // platform. It is however guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), boost::json::parse(kLedgerExpected));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, Version2)
{
    static auto const kLedgerExpected = R"JSON({
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
    })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    // When 'type' not specified, default to all the types
    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limitLine--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kAccount, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({"limit": 10})JSON");
        auto output = handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per
        // platform. It is however guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), boost::json::parse(kLedgerExpected));
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilter)
{
    static auto const kLedgerExpected = R"JSON({
        "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "close_flags": 0,
        "close_time": 0,
        "close_time_resolution": 0,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": "30",
        "parent_close_time": 0,
        "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "total_coins": "0",
        "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "closed": true
    })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limitLine = 5;
    auto limitTicket = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + limitTicket);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limitLine--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    while ((limitTicket--) != 0) {
        auto const ticket = createTicketLedgerObject(kAccount, limitTicket);
        bbs.push_back(ticket.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({
            "limit": 10,
            "type": "state"
        })JSON");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per
        // platform. It is however guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), boost::json::parse(kLedgerExpected));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 5);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterAMM)
{
    static auto const kLedgerExpected = R"JSON({
        "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "close_flags": 0,
        "close_time": 0,
        "close_time_resolution": 0,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": "30",
        "parent_close_time": 0,
        "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "total_coins": "0",
        "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "closed": true
    })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limitLine = 5;

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limitLine + 1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limitLine--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    auto const amm =
        createAmmObject(kAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "JPY", kAccount2);
    bbs.push_back(amm.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({
            "limit": 6,
            "type": "amm"
        })JSON");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));

        // Note: the format of "close_time_human" depends on the platform and might differ per
        // platform. It is however guaranteed to be consistent on the same platform.
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), boost::json::parse(kLedgerExpected));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, OutOfOrder)
{
    static auto const kLedgerExpected = R"JSON({
        "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "close_flags": 0,
        "close_time": 0,
        "close_time_resolution": 0,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": "30",
        "parent_close_time": 0,
        "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "total_coins": "0",
        "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "closed": true
    })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    // page end
    // marker return seq
    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(2);
    ON_CALL(*backend_, doFetchSuccessorKey(kFirstKey, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));
    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kIndex2}, kRangeMax, _))
        .WillByDefault(Return(std::nullopt));

    auto const line = createRippleStateLedgerObject(
        "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
    );
    bbs.push_back(line.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({"limit": 10, "out_of_order": true})JSON");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(output.result->as_object().at("ledger"), boost::json::parse(kLedgerExpected));
        EXPECT_EQ(output.result->as_object().at("marker").as_uint64(), kRangeMax);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, Marker)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillByDefault(Return(createRippleStateLedgerObject(
                                  "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        )
                                  .getSerializer()
                                  .peekData()));

    auto limit = 10;
    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limit);
    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));
    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kIndex2}, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limit--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "limit": 10,
                    "marker": "{}"
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, DiffMarker)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limit = 10;
    std::vector<LedgerObject> los;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, fetchLedgerDiff).Times(1);

    while ((limit--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
        los.emplace_back(
            LedgerObject{.key = xrpl::uint256{kIndex2}, .blob = Blob{}}
        );  // NOLINT(modernize-use-emplace)
    }
    ON_CALL(*backend_, fetchLedgerDiff(kRangeMax, _)).WillByDefault(Return(los));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "limit": 10,
                    "marker": {},
                    "out_of_order": true
                }})JSON",
                kRangeMax
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
        EXPECT_FALSE(output.result->as_object().at("cache_full").as_bool());
    });
}

TEST_F(RPCLedgerDataHandlerTest, Binary)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limit = 10;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(limit);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limit--) != 0) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            R"JSON({
                "limit": 10,
                "binary": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, BinaryLimitMoreThanMax)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limit = LedgerDataHandler::kLimitBinary + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(LedgerDataHandler::kLimitBinary);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limit--) != 0u) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "limit": {},
                    "binary": true
                }})JSON",
                LedgerDataHandler::kLimitBinary + 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().contains("ledger_data"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(
            output.result->as_object().at("state").as_array().size(),
            LedgerDataHandler::kLimitBinary
        );
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, JsonLimitMoreThanMax)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    auto limit = LedgerDataHandler::kLimitJson + 1;
    std::vector<Blob> bbs;

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(LedgerDataHandler::kLimitJson);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    while ((limit--) != 0u) {
        auto const line = createRippleStateLedgerObject(
            "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
        );
        bbs.push_back(line.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "limit": {},
                    "binary": false
                }})JSON",
                LedgerDataHandler::kLimitJson + 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_TRUE(output.result->as_object().at("ledger").as_object().at("closed").as_bool());
        EXPECT_EQ(
            output.result->as_object().at("state").as_array().size(), LedgerDataHandler::kLimitJson
        );
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterMPTIssuance)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    auto const issuance = createMptIssuanceObject(kAccount, 2, "metadata");
    bbs.push_back(issuance.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({
            "limit": 1,
            "type": "mpt_issuance"
        })JSON");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);

        auto const& objects = output.result->as_object().at("state").as_array();
        EXPECT_EQ(objects.front().at("LedgerEntryType").as_string(), "MPTokenIssuance");

        // make sure mptID is synethetically parsed if object is mptIssuance
        EXPECT_EQ(
            objects.front().at("mpt_issuance_id").as_string(),
            xrpl::to_string(xrpl::makeMptID(2, getAccountIdWithString(kAccount)))
        );
    });
}

TEST_F(RPCLedgerDataHandlerTest, TypeFilterMPToken)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillByDefault(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    std::vector<Blob> bbs;
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);
    ON_CALL(*backend_, doFetchSuccessorKey(_, kRangeMax, _))
        .WillByDefault(Return(xrpl::uint256{kIndex2}));

    auto const mptoken =
        createMpTokenObject(kAccount, xrpl::makeMptID(2, getAccountIdWithString(kAccount)));
    bbs.push_back(mptoken.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerDataHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({
            "limit": 1,
            "type": "mptoken"
        })JSON");

        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
        EXPECT_EQ(output.result->as_object().at("state").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), kIndex2);
        EXPECT_EQ(output.result->as_object().at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), kRangeMax);

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
    EXPECT_EQ(
        warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated)
    );
    EXPECT_NE(
        warning.at("message").as_string().find("Field 'ledger' is deprecated."), std::string::npos
    ) << warning;
}
