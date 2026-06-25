#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Ledger.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;
constexpr auto kApiVersion = 2;

}  // namespace

using namespace rpc;
using namespace data;
using namespace testing;

struct RPCLedgerHandlerTest : HandlerBaseTest {
    RPCLedgerHandlerTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

struct LedgerParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerParameterTest : public RPCLedgerHandlerTest,
                             public WithParamInterface<LedgerParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<LedgerParamTestCaseBundle>{
        {
            .testName = "AccountsInvalidBool",
            .testJson = R"JSON({"accounts": true})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Not supported field 'accounts's value 'true'",
        },
        {
            .testName = "AccountsInvalidInt",
            .testJson = R"JSON({"accounts": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "FullInvalidBool",
            .testJson = R"JSON({"full": true})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Not supported field 'full's value 'true'",
        },
        {
            .testName = "FullInvalidInt",
            .testJson = R"JSON({"full": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "QueueExist",
            .testJson = R"JSON({"queue": true})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Not supported field 'queue's value 'true'",
        },
        {
            .testName = "QueueNotBool",
            .testJson = R"JSON({"queue": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "OwnerFundsNotBool",
            .testJson = R"JSON({"owner_funds": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "LedgerHashInvalid",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed",
        },
        {
            .testName = "LedgerHashNotString",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString",
        },
        {
            .testName = "LedgerIndexNotInt",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed",
        },
        {
            .testName = "TransactionsNotBool",
            .testJson = R"JSON({"transactions": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "ExpandNotBool",
            .testJson = R"JSON({"expand": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "BinaryNotBool",
            .testJson = R"JSON({"binary": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "DiffNotBool",
            .testJson = R"JSON({"diff": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerGroup1,
    LedgerParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(LedgerParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
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

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
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

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
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

TEST_F(RPCLedgerHandlerTest, Default)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "30",
                "parent_close_time": 0,
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000"
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse("{}");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is slightly different cross the platform
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

// fields not supported for specific value can be set to its default value
TEST_F(RPCLedgerHandlerTest, ConditionallyNotSupportedFieldsDefaultValue)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "full": false,
                "accounts": false,
                "queue": false
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCLedgerHandlerTest, QueryViaLedgerIndex)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(15, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(R"JSON({"ledger_index": 15})JSON");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
    });
}

TEST_F(RPCLedgerHandlerTest, QueryViaLedgerHash)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kIndex1}, _))
        .WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"ledger_hash": "{}" }})JSON", kIndex1));
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("ledger"));
    });
}

TEST_F(RPCLedgerHandlerTest, BinaryTrue)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "closed": true
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandBinary)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "closed": true,
                "transactions": [
                    {
                        "tx_blob": "120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta": "201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    },
                    {
                        "tx_blob": "120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta": "201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    }
                ]
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{t1, t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandBinaryV2)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "closed": true,
                "transactions": [
                    {
                        "hash": "70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "tx_blob": "120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta_blob": "201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    },
                    {
                        "hash": "70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "tx_blob": "120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta_blob": "201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    }
                ]
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillOnce(Return(ledgerHeader));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillOnce(Return(std::vector{t1, t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true
            })JSON"
        );
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandNotBinary)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "30",
                "parent_close_time": 0,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "100",
                        "DeliverMax": "100",
                        "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee": "3",
                        "Sequence": 30,
                        "SigningPubKey": "74657374",
                        "TransactionType": "Payment",
                        "hash": "70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "metaData": {
                        "AffectedNodes": [
                            {
                                "ModifiedNode": {
                                    "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "110"
                                    },
                                    "LedgerEntryType": "AccountRoot"
                                }
                            },
                            {
                                "ModifiedNode": {
                                    "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "30"
                                    },
                                    "LedgerEntryType": "AccountRoot"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "delivered_amount": "unavailable"
                        }
                    }
                ]
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": false,
                "expand": true,
                "transactions": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is slightly different cross the platform
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandNotBinaryV2)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": 30,
                "parent_close_time": 0,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "validated": true,
                        "close_time_iso": "2000-01-01T00:00:00Z",
                        "hash": "70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                        "ledger_index": 30,
                        "tx_json": {
                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                            "DeliverMax": "100",
                            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "Fee": "3",
                            "Sequence": 30,
                            "SigningPubKey": "74657374",
                            "TransactionType": "Payment"
                        },
                        "meta": {
                            "AffectedNodes": [
                                {
                                    "ModifiedNode": {
                                        "FinalFields": {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Balance": "110"
                                        },
                                        "LedgerEntryType": "AccountRoot"
                                    }
                                },
                                {
                                    "ModifiedNode": {
                                        "FinalFields": {
                                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                            "Balance": "30"
                                        },
                                        "LedgerEntryType": "AccountRoot"
                                    }
                                }
                            ],
                            "TransactionIndex": 0,
                            "TransactionResult": "tesSUCCESS",
                            "delivered_amount": "unavailable"
                        }
                    }
                ]
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillOnce(Return(ledgerHeader));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillOnce(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": false,
                "expand": true,
                "transactions": true
            })JSON"
        );
        auto output = handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        // remove human readable time, it is slightly different cross the platform
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TwoRequestInARowTransactionsExpandNotBinaryV2)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillOnce(Return(ledgerHeader));

    auto const ledgerHeader2 = createLedgerHeader(kLedgerHash, kRangeMax - 1, 10);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax - 1, _)).WillOnce(Return(ledgerHeader2));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillOnce(Return(std::vector{t1}));
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax - 1, _))
        .WillOnce(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": false,
                "expand": true,
                "transactions": true
            })JSON"
        );
        auto output = handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);

        auto const req2 = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "binary": false,
                    "expand": true,
                    "transactions": true,
                    "ledger_index": {}
                }})JSON",
                kRangeMax - 1
            )
        );
        auto output2 = handler.process(req2, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output2);
        EXPECT_NE(
            output.result->at("ledger").at("transactions").as_array()[0].at("close_time_iso"),
            output2.result->at("ledger").at("transactions").as_array()[0].at("close_time_iso")
        );
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsNotExpand)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    EXPECT_CALL(*backend_, fetchAllTransactionHashesInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionHashesInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "transactions": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("ledger").at("transactions"),
            boost::json::parse(fmt::format(R"JSON(["{}", "{}"])JSON", kIndex1, kIndex2))
        );
    });
}

TEST_F(RPCLedgerHandlerTest, DiffNotBinary)
{
    static constexpr auto kExpectedOut =
        R"JSON([
            {
                "object_id": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1",
                "object": ""
            },
            {
                "object_id": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "object": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Balance": "10",
                "Flags": 4194304,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "PreviousTxnLgrSeq": 3,
                "Sequence": 1,
                "TransferRate": 0,
                "index": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                }
            }
        ])JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    std::vector<LedgerObject> los;

    EXPECT_CALL(*backend_, fetchLedgerDiff).Times(1);

    los.push_back(
        LedgerObject{.key = xrpl::uint256{kIndex2}, .blob = Blob{}}
    );  // NOLINT(modernize-use-emplace)
    los.push_back(
        LedgerObject{
            .key = xrpl::uint256{kIndex1},
            .blob = createAccountRootObject(kAccount, xrpl::lsfGlobalFreeze, 1, 10, 2, kIndex1, 3)
                        .getSerializer()
                        .peekData()
        }
    );

    ON_CALL(*backend_, fetchLedgerDiff(kRangeMax, _)).WillByDefault(Return(los));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "diff": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger").at("diff"), boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, DiffBinary)
{
    static constexpr auto kExpectedOut =
        R"JSON([
            {
                "object_id": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1",
                "object": ""
            },
            {
                "object_id": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "object": "1100612200400000240000000125000000032B000000002D00000002551B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC62400000000000000A81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
            }
        ])JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    std::vector<LedgerObject> los;

    EXPECT_CALL(*backend_, fetchLedgerDiff).Times(1);

    los.push_back(
        LedgerObject{.key = xrpl::uint256{kIndex2}, .blob = Blob{}}
    );  // NOLINT(modernize-use-emplace)
    los.push_back(
        LedgerObject{
            .key = xrpl::uint256{kIndex1},
            .blob = createAccountRootObject(kAccount, xrpl::lsfGlobalFreeze, 1, 10, 2, kIndex1, 3)
                        .getSerializer()
                        .peekData()
        }
    );

    ON_CALL(*backend_, fetchLedgerDiff(kRangeMax, _)).WillByDefault(Return(los));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "diff": true,
                "binary": true
            })JSON"
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger").at("diff"), boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsEmpty)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "ledger": {
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "30",
                "parent_close_time": 0,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "100",
                        "DeliverMax": "100",
                        "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee": "3",
                        "Sequence": 30,
                        "SigningPubKey": "74657374",
                        "TransactionType": "Payment",
                        "hash": "70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "metaData": {
                        "AffectedNodes": [
                            {
                                "ModifiedNode": {
                                    "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "110"
                                    },
                                    "LedgerEntryType": "AccountRoot"
                                }
                            },
                            {
                                "ModifiedNode": {
                                    "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "30"
                                    },
                                    "LedgerEntryType": "AccountRoot"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "delivered_amount": "unavailable"
                        }
                    }
                ]
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, 100, 3, kRangeMax)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": false,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is slightly different cross the platform
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsTrueBinaryFalse)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger": {
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "30",
                "parent_close_time": 0,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "2",
                        "hash": "65757B01CC1DF860DC6FEC73D6435D902BDC5E52D3FCB519E83D91C1F3D82EDC",
                        "metaData": {
                            "AffectedNodes": [
                                {
                                    "CreatedNode": {
                                        "LedgerEntryType": "Offer",
                                        "NewFields": {
                                            "TakerGets": "300",
                                            "TakerPays": {
                                                "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                "value": "200"
                                            }
                                        }
                                    }
                                }
                            ],
                            "TransactionIndex": 100,
                            "TransactionResult": "tesSUCCESS"
                        },
                        "owner_funds": "193",
                        "Sequence": 100,
                        "SigningPubKey": "74657374",
                        "TakerGets": "300",
                        "TakerPays": {
                            "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "value": "200"
                        },
                        "TransactionType": "OfferCreate"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // account doFetchLedgerObject
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    auto const accountObject =
        createAccountRootObject(
            kAccount, 0, kRangeMax, 200 /*balance*/, 2 /*owner object*/, kIndex1, kRangeMax - 1, 0
        )
            .getSerializer()
            .peekData();
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kRangeMax, _))
        .WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 200 - 7 -> 193
    auto feeBlob = createLegacyFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kRangeMax, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCurrency, kAccount2, 100, 300, 200)
                      .getSerializer()
                      .peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300, true)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": false,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is slightly different cross the platform
        EXPECT_EQ(output.result->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsTrueBinaryTrue)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger": {
                "closed": true,
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "meta": "201C00000064F8E311006FE864D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012CE1E1F1031000",
                        "owner_funds": "193",
                        "tx_blob": "120007240000006464D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012C68400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // account doFetchLedgerObject
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    auto const accountObject =
        createAccountRootObject(
            kAccount, 0, kRangeMax, 200 /*balance*/, 2 /*owner object*/, kIndex1, kRangeMax - 1, 0
        )
            .getSerializer()
            .peekData();
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kRangeMax, _))
        .WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 200 - 7 -> 193
    auto feeBlob = createLegacyFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kRangeMax, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCurrency, kAccount2, 100, 300, 200)
                      .getSerializer()
                      .peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300, true)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsIssuerIsSelf)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // issuer is self
    TransactionAndMetadata tx;
    tx.metadata =
        createMetaDataForCreateOffer(kCurrency, kAccount, 100, 300, 200).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object()["ledger"]
                         .as_object()["transactions"]
                         .as_array()[0]
                         .as_object()
                         .contains("owner_funds"));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsNotEnoughForReserve)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger": {
                "closed": true,
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "meta": "201C00000064F8E311006FE864D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012CE1E1F1031000",
                        "owner_funds": "0",
                        "tx_blob": "120007240000006464D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012C68400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // account doFetchLedgerObject
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    auto const accountObject =
        createAccountRootObject(
            kAccount, 0, kRangeMax, 6 /*balance*/, 2 /*owner object*/, kIndex1, kRangeMax - 1, 0
        )
            .getSerializer()
            .peekData();
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kRangeMax, _))
        .WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 6 - 7 -> -1
    auto feeBlob = createLegacyFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kRangeMax, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCurrency, kAccount2, 100, 300, 200)
                      .getSerializer()
                      .peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300, true)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsNotXRP)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // mock line
    auto const line = createRippleStateLedgerObject(
        kCurrency, kAccount2, 50 /*balance*/, kAccount, 10, kAccount2, 20, kIndex1, 123
    );
    auto lineKey = xrpl::keylet::line(
                       getAccountIdWithString(kAccount),
                       getAccountIdWithString(kAccount2),
                       xrpl::toCurrency(std::string(kCurrency))
    )
                       .key;
    ON_CALL(*backend_, doFetchLedgerObject(lineKey, kRangeMax, _))
        .WillByDefault(Return(line.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCurrency, kAccount2, 100, 300, 200, true)
                      .getSerializer()
                      .peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object()["ledger"]
                .as_object()["transactions"]
                .as_array()[0]
                .as_object()["owner_funds"]
                .as_string(),
            "50"
        );
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsIgnoreFreezeLine)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(ledgerHeader));

    // mock line freeze
    auto const line = createRippleStateLedgerObject(
        kCurrency,
        kAccount2,
        50 /*balance*/,
        kAccount,
        10,
        kAccount2,
        20,
        kIndex1,
        123,
        xrpl::lsfLowFreeze | xrpl::lsfHighFreeze
    );
    auto lineKey = xrpl::keylet::line(
                       getAccountIdWithString(kAccount),
                       getAccountIdWithString(kAccount2),
                       xrpl::toCurrency(std::string(kCurrency))
    )
                       .key;
    ON_CALL(*backend_, doFetchLedgerObject(lineKey, kRangeMax, _))
        .WillByDefault(Return(line.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCurrency, kAccount2, 100, 300, 200, true)
                      .getSerializer()
                      .peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = kRangeMax;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*backend_, fetchAllTransactionsInLedger(kRangeMax, _))
        .WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(
            R"JSON({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })JSON"
        );
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object()["ledger"]
                .as_object()["transactions"]
                .as_array()[0]
                .as_object()["owner_funds"]
                .as_string(),
            "50"
        );
    });
}
struct RPCLedgerHandlerSpecCheckTestBundle {
    std::string name;
    boost::json::value json;
    std::unordered_map<int64_t, std::vector<std::string>> expectedWarning;
};

struct RPCLedgerHandlerSpecCheckTest
    : ::testing::TestWithParam<RPCLedgerHandlerSpecCheckTestBundle> {
    RpcSpec spec = LedgerHandler::spec(2);
};

INSTANTIATE_TEST_SUITE_P(
    RPCLedgerHandlerSpecCheckTestGroup,
    RPCLedgerHandlerSpecCheckTest,
    testing::Values(
        RPCLedgerHandlerSpecCheckTestBundle{"ValidRequest", {{"ledger_index", 1}}, {}},
        RPCLedgerHandlerSpecCheckTestBundle{
            "FullWarning",
            {{JS(full), false}},
            {{static_cast<int64_t>(WarningCode::WarnRpcDeprecated),
              {"Field 'full' is deprecated."}}},
        },
        RPCLedgerHandlerSpecCheckTestBundle{
            "AccountsWarning",
            {{JS(accounts), false}},
            {{static_cast<int64_t>(WarningCode::WarnRpcDeprecated),
              {"Field 'accounts' is deprecated."}}},
        },
        RPCLedgerHandlerSpecCheckTestBundle{
            "LedgerWarning",
            {{JS(ledger), false}},
            {{static_cast<int64_t>(WarningCode::WarnRpcDeprecated),
              {"Field 'ledger' is deprecated."}}},
        },
        RPCLedgerHandlerSpecCheckTestBundle{
            "TypeWarning",
            {{JS(type), false}},
            {{static_cast<int64_t>(WarningCode::WarnRpcDeprecated),
              {"Field 'type' is deprecated."}}},
        },
        RPCLedgerHandlerSpecCheckTestBundle{
            "MultipleWarnings",
            {{JS(full), false}, {JS(type), false}},
            {{static_cast<int64_t>(WarningCode::WarnRpcDeprecated),
              {"Field 'full' is deprecated.", "Field 'type' is deprecated."}}},
        }
    ),
    [](testing::TestParamInfo<RPCLedgerHandlerSpecCheckTestBundle> const& info) {
        return info.param.name;
    }
);

TEST_P(RPCLedgerHandlerSpecCheckTest, CheckSpec)
{
    auto const warnings = spec.check(GetParam().json);
    ASSERT_EQ(warnings.size(), GetParam().expectedWarning.size());
    for (auto const& warn : warnings) {
        ASSERT_TRUE(warn.is_object());
        auto const obj = warn.as_object();
        ASSERT_TRUE(obj.contains("id"));
        ASSERT_TRUE(obj.contains("message"));
        auto const& it = GetParam().expectedWarning.find(obj.at("id").as_int64());
        ASSERT_NE(it, GetParam().expectedWarning.end());
        for (auto const& msg : it->second) {
            EXPECT_NE(obj.at("message").as_string().find(msg), std::string::npos);
        }
    }
}
