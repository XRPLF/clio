#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/BookChanges.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STObject.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kAccount1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kDomain = "F10D0CC9A0F9A3CBF585B80BE09A186483668FDBDD39AA7E3370F3649CE134E5";
constexpr auto kMaxSeq = 30;
constexpr auto kMinSeq = 10;

}  // namespace

struct RPCBookChangesHandlerTest : HandlerBaseTest {
    RPCBookChangesHandlerTest()
    {
        backend_->setRange(kMinSeq, kMaxSeq);
    }
};

struct BookChangesParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct BookChangesParameterTest : public RPCBookChangesHandlerTest,
                                  public WithParamInterface<BookChangesParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<BookChangesParamTestCaseBundle>{
        BookChangesParamTestCaseBundle{
            .testName = "LedgerHashInvalid",
            .testJson = R"JSON({"ledger_hash": "1"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        BookChangesParamTestCaseBundle{
            .testName = "LedgerHashNotString",
            .testJson = R"JSON({"ledger_hash": 1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        BookChangesParamTestCaseBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = R"JSON({"ledger_index": "a"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCBookChangesGroup1,
    BookChangesParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(BookChangesParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{BookChangesHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCBookChangesHandlerTest, LedgerNonExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kMaxSeq, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(R"JSON({"ledger_index": 30})JSON");
    auto const handler = AnyHandler{BookChangesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookChangesHandlerTest, LedgerNonExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kMaxSeq, _)).WillByDefault(Return(std::nullopt));

    static auto const kInput = boost::json::parse(R"JSON({"ledger_index": "30"})JSON");
    auto const handler = AnyHandler{BookChangesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookChangesHandlerTest, LedgerNonExistViaHash)
{
    // return empty ledgerHeader
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_hash": "{}"
            }})JSON",
            kLedgerHash
        )
    );
    auto const handler = AnyHandler{BookChangesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookChangesHandlerTest, NormalPath)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "type": "bookChanges",
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "ledger_time": 0,
            "validated": true,
            "changes": [
                {
                    "currency_a": "XRP_drops",
                    "currency_b": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD/0158415500000000C1F76FF6ECB0BAC600000000",
                    "volume_a": "2",
                    "volume_b": "2",
                    "high": "-1",
                    "low": "-1",
                    "open": "-1",
                    "close": "-1"
                }
            ]
        })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence(kMaxSeq, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kMaxSeq)));

    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STObject const metaObj = createMetaDataForBookChange(kCurrency, kIssuer, 22, 1, 3, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    transactions.push_back(trans1);

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kMaxSeq, _)).WillOnce(Return(transactions));

    auto const handler = AnyHandler{BookChangesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(boost::json::parse("{}"), Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCBookChangesHandlerTest, NormalPathWithDomain)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "type": "bookChanges",
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "ledger_time": 0,
            "validated": true,
            "changes": [
                {
                    "currency_a": "XRP_drops",
                    "currency_b": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD/0158415500000000C1F76FF6ECB0BAC600000000",
                    "volume_a": "2",
                    "volume_b": "2",
                    "high": "-1",
                    "low": "-1",
                    "open": "-1",
                    "close": "-1",
                    "domain": "F10D0CC9A0F9A3CBF585B80BE09A186483668FDBDD39AA7E3370F3649CE134E5"
                }
            ]
        })JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence(kMaxSeq, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kMaxSeq)));

    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STObject const metaObj =
        createMetaDataForBookChange(kCurrency, kIssuer, 22, 1, 3, 3, 1, kDomain);
    trans1.metadata = metaObj.getSerializer().peekData();
    transactions.push_back(trans1);

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kMaxSeq, _)).WillOnce(Return(transactions));

    auto const handler = AnyHandler{BookChangesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(boost::json::parse("{}"), Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}
