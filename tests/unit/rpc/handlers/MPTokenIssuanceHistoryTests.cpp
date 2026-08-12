#include "data/Types.hpp"
#include "migration/MigratiorStatus.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/MPTokenIssuanceHistory.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {
constexpr auto kMinSeq = 10;
constexpr auto kMaxSeq = 30;
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
// Valid 48-hex MPT issuance ID (from MPTHoldersTests.cpp)
constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405B0E3E";
constexpr auto kMptIdLowercase = "000004c463c52827307480341125da0577defc38405b0e3e";
constexpr auto kApiVersion = 2;

auto const kMigratedStatus =
    migration::MigratorStatus{migration::MigratorStatus::Status::Migrated}.toString();
auto const kNotMigratedStatus =
    migration::MigratorStatus{migration::MigratorStatus::Status::NotMigrated}.toString();

}  // namespace

struct RPCMPTokenIssuanceHistoryHandlerTest : HandlerBaseTest {
    RPCMPTokenIssuanceHistoryHandlerTest()
    {
        backend_->setRange(kMinSeq, kMaxSeq);
        ON_CALL(*backend_, fetchMigratorStatus)
            .WillByDefault(Return(std::optional<std::string>{kMigratedStatus}));
    }
};

/**
 * @brief Enables handler log capture below the test runner's default fatal severity.
 */
struct RPCMPTokenIssuanceHistoryHandlerLogTest : RPCMPTokenIssuanceHistoryHandlerTest,
                                                 LoggerFixture {};

struct MPTokenIssuanceHistoryParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct MPTokenIssuanceHistoryParameterTest
    : public RPCMPTokenIssuanceHistoryHandlerTest,
      public WithParamInterface<MPTokenIssuanceHistoryParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<MPTokenIssuanceHistoryParamTestCaseBundle>{
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "MissingMptIssuanceID",
            .testJson = R"JSON({})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'mpt_issuance_id' missing"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "MalformedMptIssuanceID",
            .testJson = R"JSON({"mpt_issuance_id": "NOTAHEXSTRING"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "mpt_issuance_idMalformed"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "BinaryNotBool",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "binary": 1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "ForwardNotBool",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "forward": 1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMinNotInt",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "ledger_index_min": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMaxNotInt",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "ledger_index_max": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "BadAccount",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "account": "not_a_valid_account"})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "UnknownTxType",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "tx_type": "NotARealType"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'tx_type'."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "MarkerNotObject",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "marker": 101})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "invalidMarker"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "MarkerMissingSeq",
            .testJson = R"JSON({
                "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E",
                "marker": {"ledger": 123}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'seq' missing"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "MarkerMissingLedger",
            .testJson = R"JSON({
                "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E",
                "marker": {"seq": 123}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'ledger' missing"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LedgerIndexInvalid",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "ledger_index": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LedgerHashInvalid",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "ledger_hash": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LedgerHashNotString",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "ledger_hash": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LimitNotInt",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "limit": "123"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LimitNegative",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "limit": -1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        MPTokenIssuanceHistoryParamTestCaseBundle{
            .testName = "LimitZero",
            .testJson =
                R"JSON({"mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E", "limit": 0})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCMPTokenIssuanceHistoryGroup1,
    MPTokenIssuanceHistoryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(MPTokenIssuanceHistoryParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexMinOutOfRange)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": 9
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrIdxMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerSeqMinOutOfRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexMaxOutOfRange)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_max": 31
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrIdxMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerSeqMaxOutOfRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, InvertedLedgerRange)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": 20,
                    "ledger_index_max": 11
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrIdxsInvalid");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ContainsLedgerSpecifierAndRange)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": 11,
                    "ledger_index_max": 20,
                    "ledger_index": 10
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "containsLedgerSpecifierAndRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateNotMigratedReturnsNotReady)
{
    ON_CALL(*backend_, fetchMigratorStatus)
        .WillByDefault(Return(std::optional<std::string>{kNotMigratedStatus}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "notReady");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateMissingStatusReturnsNotReady)
{
    ON_CALL(*backend_, fetchMigratorStatus).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "notReady");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateUnknownStatusStringReturnsNotReady)
{
    ON_CALL(*backend_, fetchMigratorStatus)
        .WillByDefault(Return(std::optional<std::string>{"NotAStatus"}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "notReady");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateNotReadyErrorMessageContent)
{
    ON_CALL(*backend_, fetchMigratorStatus)
        .WillByDefault(Return(std::optional<std::string>{kNotMigratedStatus}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "notReady");
        auto const msg = err.at("error_message").as_string();
        EXPECT_TRUE(msg.find("backfill has not completed") != std::string::npos);
        // The remedy is the operator's, not the caller's: no CLI invocation in a client-facing
        // error.
        EXPECT_TRUE(msg.find("clio_server") == std::string::npos);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateMigratedServesRequest)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("mpt_issuance_id"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateCachedMigratedShortCircuit)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus(MPTokenIssuanceHistoryHandler::kMigratorName, _))
        .Times(1)
        .WillOnce(Return(std::optional<std::string>{kMigratedStatus}));

    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    // Same handler instance across both calls: the cached Migrated flag skips the second check.
    auto anyHandler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};

    runSpawn([&](auto yield) {
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output1 = anyHandler.process(req, Context{yield});
        ASSERT_TRUE(output1);
        auto const output2 = anyHandler.process(req, Context{yield});
        ASSERT_TRUE(output2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, GateCachedMigratedSurvivesHandlerCopy)
{
    EXPECT_CALL(*backend_, fetchMigratorStatus(MPTokenIssuanceHistoryHandler::kMigratorName, _))
        .Times(1)
        .WillOnce(Return(std::optional<std::string>{kMigratedStatus}));

    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    // Every request gets a copy of the registered handler, so the cached flag survives the copy.
    auto original = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
    auto const copy = original;

    runSpawn([&](auto yield) {
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = original.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const outputFromCopy = copy.process(req, Context{yield});
        ASSERT_TRUE(outputFromCopy);
    });
}

static std::vector<TransactionAndMetadata>
genTransactions(uint32_t seq1, uint32_t seq2)
{
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj1 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans1.transaction = obj1.getSerializer().peekData();
    trans1.ledgerSequence = seq1;
    xrpl::STObject const meta1 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans1.metadata = meta1.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto trans2 = TransactionAndMetadata();
    xrpl::STObject const obj2 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans2.transaction = obj2.getSerializer().peekData();
    trans2.ledgerSequence = seq2;
    xrpl::STObject const meta2 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans2.metadata = meta2.getSerializer().peekData();
    trans2.date = 2;
    transactions.push_back(trans2);

    return transactions;
}

// Build a mixed-type page: one Payment (at seqPayment) and one OfferCreate (at seqOffer).
static std::vector<TransactionAndMetadata>
genMixedTypeTransactions(uint32_t seqPayment, uint32_t seqOffer)
{
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto payment = TransactionAndMetadata();
    xrpl::STObject const paymentObj = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    payment.transaction = paymentObj.getSerializer().peekData();
    payment.ledgerSequence = seqPayment;
    xrpl::STObject const paymentMeta =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    payment.metadata = paymentMeta.getSerializer().peekData();
    payment.date = 1;
    transactions.push_back(payment);

    auto offer = TransactionAndMetadata();
    xrpl::STObject const offerObj =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300);
    offer.transaction = offerObj.getSerializer().peekData();
    offer.ledgerSequence = seqOffer;
    xrpl::STObject const offerMeta =
        createMetaDataForCreateOffer(kCurrency, kAccount, 100, 200, 300);
    offer.metadata = offerMeta.getSerializer().peekData();
    offer.date = 2;
    transactions.push_back(offer);

    return transactions;
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, RoutingWithoutAccountCallsFetchMPTIssuanceTxns)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    EXPECT_CALL(*backend_, fetchMPTokenIssuanceTransactions).Times(1);
    EXPECT_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).Times(0);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, RoutingWithAccountCallsFetchAccountMPTIssuanceTxns)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    EXPECT_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).Times(1);
    EXPECT_CALL(*backend_, fetchMPTokenIssuanceTransactions).Times(0);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(R"JSON({{"mpt_issuance_id": "{}", "account": "{}"}})JSON", kMptId, kAccount)
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ForwardCursorSeedFromMinIndex)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, true, testing::Optional(testing::Eq(TransactionsCursor{kMinSeq + 1, 0})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": true
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMinSeq + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ReverseCursorSeedFromMaxIndex)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, NoIndexSpecifiedForwardSeedsFromGlobalMin)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, true, testing::Optional(testing::Eq(TransactionsCursor{kMinSeq, 0})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "forward": true
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMinSeq);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, NoIndexSpecifiedReverseSeedsFromGlobalMax)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, false, testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq, INT32_MAX})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "forward": false
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMinSeq);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, IndexSpecificForwardFalseV1)
{
    constexpr auto kOutput = R"JSON({
        "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E",
        "ledger_index_min": 11,
        "ledger_index_max": 29,
        "transactions": [
            {
                "meta": {
                    "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }
                    ],
                    "TransactionIndex": 0,
                    "TransactionResult": "tesSUCCESS",
                    "delivered_amount": "unavailable"
                },
                "tx": {
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Amount": "1",
                    "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "Fee": "1",
                    "Sequence": 32,
                    "SigningPubKey": "74657374",
                    "TransactionType": "Payment",
                    "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                    "DeliverMax": "1",
                    "ledger_index": 11,
                    "date": 1
                },
                "validated": true
            },
            {
                "meta": {
                    "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }
                    ],
                    "TransactionIndex": 0,
                    "TransactionResult": "tesSUCCESS",
                    "delivered_amount": "unavailable"
                },
                "tx": {
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Amount": "1",
                    "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "Fee": "1",
                    "Sequence": 32,
                    "SigningPubKey": "74657374",
                    "TransactionType": "Payment",
                    "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                    "DeliverMax": "1",
                    "ledger_index": 29,
                    "date": 2
                },
                "validated": true
            }
        ],
        "validated": true,
        "marker": {
            "ledger": 12,
            "seq": 34
        }
    })JSON";

    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(kOutput));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, IndexSpecificForwardFalseV2)
{
    constexpr auto kOutput = R"JSON({
        "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405B0E3E",
        "ledger_index_min": 11,
        "ledger_index_max": 29,
        "transactions": [
            {
                "meta": {
                    "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }
                    ],
                    "TransactionIndex": 0,
                    "TransactionResult": "tesSUCCESS",
                    "delivered_amount": "unavailable"
                },
                "tx_json": {
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "Fee": "1",
                    "Sequence": 32,
                    "SigningPubKey": "74657374",
                    "TransactionType": "Payment",
                    "DeliverMax": "1",
                    "ledger_index": 11,
                    "date": 1
                },
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "ledger_index": 11,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "validated": true
            },
            {
                "meta": {
                    "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }
                    ],
                    "TransactionIndex": 0,
                    "TransactionResult": "tesSUCCESS",
                    "delivered_amount": "unavailable"
                },
                "tx_json": {
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "Fee": "1",
                    "Sequence": 32,
                    "SigningPubKey": "74657374",
                    "TransactionType": "Payment",
                    "DeliverMax": "1",
                    "ledger_index": 29,
                    "date": 2
                },
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "ledger_index": 29,
                "close_time_iso": "2000-01-01T00:00:00Z",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "validated": true
            }
        ],
        "validated": true,
        "marker": {
            "ledger": 12,
            "seq": 34
        }
    })JSON";

    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .WillOnce(Return(transCursor));

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(2);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 1
            )
        );
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(kOutput));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, BinaryTrueV1)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, false, testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq, INT32_MAX})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "binary": true
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMinSeq);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        auto const& firstTx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_TRUE(firstTx.contains("tx_blob"));
        EXPECT_TRUE(firstTx.contains("meta"));
        EXPECT_TRUE(firstTx.contains("ledger_index"));
        EXPECT_TRUE(firstTx.contains("date"));
        EXPECT_TRUE(firstTx.contains("validated"));
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, BinaryTrueV2)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, false, testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq, INT32_MAX})), _
        )
    )
        .WillOnce(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "binary": true
                }})JSON",
                kMptId
            )
        );
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        auto const& firstTx = output.result->at("transactions").as_array()[0].as_object();
        // V2 binary uses meta_blob instead of meta
        EXPECT_TRUE(firstTx.contains("tx_blob"));
        EXPECT_TRUE(firstTx.contains("meta_blob"));
        EXPECT_TRUE(firstTx.contains("ledger_index"));
        EXPECT_TRUE(firstTx.contains("date"));
        EXPECT_TRUE(firstTx.contains("validated"));
        EXPECT_EQ(
            firstTx.at("meta_blob").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000"
        );
        EXPECT_EQ(
            firstTx.at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3"
        );
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LimitAndMarkerRoundTrip)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "limit": 2,
                    "forward": false,
                    "marker": {{"ledger": 10, "seq": 11}}
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_EQ(output.result->at("limit").as_uint64(), 2);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LimitMoreThanMax)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false,
                    "limit": {}
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 1,
                MPTokenIssuanceHistoryHandler::kLimitMax + 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("limit").as_uint64(), MPTokenIssuanceHistoryHandler::kLimitMax);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LimitNotSetDefaultUsedAndNotInResponse)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxBelowMinSeqClipped)
{
    // Reverse: first tx at kMaxSeq-1 is in range, second at kMinSeq+1 is below minIndex.
    auto const transactions = genTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false
                }})JSON",
                kMptId,
                kMinSeq + 2,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxAboveMaxSeqClipped)
{
    // Reverse: first tx at kMaxSeq-1 is above maxIndex, second at kMinSeq+1 is in range.
    auto const transactions = genTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 2, INT32_MAX})),
            _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": false
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 2
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, SpecificLedgerIndex)
{
    // reverse traversal; first tx at kMaxSeq-1 (= ledger_index), second below => dropped
    auto const transactions = genTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq - 1);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMaxSeq - 1, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index": {}
                }})JSON",
                kMptId,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, SpecificNonExistLedgerIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMaxSeq - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index": {}
                }})JSON",
                kMptId,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, SpecificLedgerHash)
{
    auto const transactions = genTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _,
            _,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMaxSeq - 1, INT32_MAX})),
            _
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq - 1);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_hash": "{}"
                }})JSON",
                kMptId,
                kLedgerHash
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, EmptyResultForUnseenId)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_TRUE(output.result->at("transactions").as_array().empty());
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, EmptyResultForUnseenIdWithAccount)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(R"JSON({{"mpt_issuance_id": "{}", "account": "{}"}})JSON", kMptId, kAccount)
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
        EXPECT_TRUE(output.result->at("transactions").as_array().empty());
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, MissingBlobMidPageSkipped)
{
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj1 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans1.transaction = obj1.getSerializer().peekData();
    trans1.ledgerSequence = kMinSeq + 1;
    xrpl::STObject const meta1 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans1.metadata = meta1.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto emptyTrans = TransactionAndMetadata();
    emptyTrans.ledgerSequence = kMinSeq + 2;
    emptyTrans.date = 2;
    transactions.push_back(emptyTrans);

    auto trans3 = TransactionAndMetadata();
    xrpl::STObject const obj3 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans3.transaction = obj3.getSerializer().peekData();
    trans3.ledgerSequence = kMaxSeq - 1;
    xrpl::STObject const meta3 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans3.metadata = meta3.getSerializer().peekData();
    trans3.date = 3;
    transactions.push_back(trans3);

    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": -1,
                    "ledger_index_max": -1,
                    "forward": false
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, MissingBlobInBinaryModeSkipped)
{
    // Page of [valid, empty, valid]: the empty record is skipped, marker unaffected.
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj1 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans1.transaction = obj1.getSerializer().peekData();
    trans1.ledgerSequence = kMaxSeq - 1;
    xrpl::STObject const meta1 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans1.metadata = meta1.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto emptyTrans = TransactionAndMetadata();
    emptyTrans.ledgerSequence = kMinSeq + 2;
    emptyTrans.date = 2;
    transactions.push_back(emptyTrans);

    auto trans3 = TransactionAndMetadata();
    xrpl::STObject const obj3 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans3.transaction = obj3.getSerializer().peekData();
    trans3.ledgerSequence = kMinSeq + 1;
    xrpl::STObject const meta3 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans3.metadata = meta3.getSerializer().peekData();
    trans3.date = 3;
    transactions.push_back(trans3);

    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{5, 6}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "binary": true
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        for (auto const& tx : output.result->at("transactions").as_array())
            EXPECT_TRUE(tx.as_object().contains("tx_blob"));
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 5, "seq": 6})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxTypeFilterSparseMixedPageKeepsMarker)
{
    // Filtering 1 of 2 yields a sparse page, but the marker rides the raw page boundary.
    auto const transactions = genMixedTypeTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).Times(0);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "tx_type": "Payment"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 1);
        auto const& tx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_EQ(tx.at("tx").as_object().at("TransactionType").as_string(), "Payment");
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxTypeFilterNonMatchReturnsEmpty)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "tx_type": "OfferCreate"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 0);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxTypeFilterCaseInsensitive)
{
    // ToLower modifier means mixed-case "pAyMeNt" still matches.
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "tx_type": "pAyMeNt"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxTypeFilterSparseMixedPageWithAccountKeepsMarker)
{
    // As the non-account sparse-page test, but via the account routing path.
    auto const transactions = genMixedTypeTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    EXPECT_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).Times(1);
    EXPECT_CALL(*backend_, fetchMPTokenIssuanceTransactions).Times(0);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "account": "{}",
                    "tx_type": "Payment"
                }})JSON",
                kMptId,
                kAccount
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 1);
        auto const& tx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_EQ(tx.at("tx").as_object().at("TransactionType").as_string(), "Payment");
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, TxTypeFilterWithAccountNonMatchReturnsEmpty)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "account": "{}",
                    "tx_type": "OfferCreate"
                }})JSON",
                kMptId,
                kAccount
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 0);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ForwardTxAboveMaxSeqClippedAndMarkerCleared)
{
    // Forward: second tx exceeds maxIndex, so it is dropped and the marker cleared.
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, true, testing::Optional(testing::Eq(TransactionsCursor{kMinSeq + 1, 0})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": true
                }})JSON",
                kMptId,
                kMinSeq + 1,
                kMaxSeq - 2
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, BinaryWithTxTypeFilterV1)
{
    // binary + tx_type: expand to filter by type, then emit the survivors in binary form.
    auto const transactions = genMixedTypeTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "binary": true,
                    "tx_type": "Payment"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 1);
        auto const& tx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_TRUE(tx.contains("tx_blob"));
        EXPECT_TRUE(tx.contains("meta"));
        EXPECT_TRUE(tx.contains("ledger_index"));
        EXPECT_TRUE(tx.contains("date"));
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, BinaryWithTxTypeFilterV2)
{
    // As above, but V2 binary uses meta_blob.
    auto const transactions = genMixedTypeTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "binary": true,
                    "tx_type": "Payment"
                }})JSON",
                kMptId
            )
        );
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 1);
        auto const& tx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_TRUE(tx.contains("tx_blob"));
        EXPECT_TRUE(tx.contains("meta_blob"));
        EXPECT_TRUE(tx.contains("ledger_index"));
        EXPECT_TRUE(tx.contains("date"));
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ResponseAlwaysHasMandatoryFields)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const& obj = output.result->as_object();
        EXPECT_TRUE(obj.contains("mpt_issuance_id"));
        EXPECT_TRUE(obj.contains("ledger_index_min"));
        EXPECT_TRUE(obj.contains("ledger_index_max"));
        EXPECT_TRUE(obj.contains("transactions"));
        EXPECT_TRUE(obj.contains("validated"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexMinAboveRangeMax)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {}
                }})JSON",
                kMptId,
                kMaxSeq + 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrIdxMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerSeqMinOutOfRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexMaxBelowRangeMin)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_max": {}
                }})JSON",
                kMptId,
                kMinSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrIdxMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerSeqMaxOutOfRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerHashWithOnlyLedgerIndexMin)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_hash": "{}",
                    "ledger_index_min": {}
                }})JSON",
                kMptId,
                kLedgerHash,
                kMinSeq + 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "containsLedgerSpecifierAndRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerHashWithOnlyLedgerIndexMax)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_hash": "{}",
                    "ledger_index_max": {}
                }})JSON",
                kMptId,
                kLedgerHash,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "containsLedgerSpecifierAndRange");
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexValidatedStringIsNotASpecifier)
{
    // "validated" resolves to no concrete index, so the request keeps the full ledger range.
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(0);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index": "validated"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMinSeq);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LedgerIndexNumericStringAccepted)
{
    auto const transactions = genTransactions(kMaxSeq - 1, kMinSeq + 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq - 1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMaxSeq - 1, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index": "{}"
                }})JSON",
                kMptId,
                kMaxSeq - 1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMaxSeq - 1);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, MissingMetadataOnlySkipped)
{
    // Page of [no metadata, valid]: the incomplete record is skipped, marker unaffected.
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto noMeta = TransactionAndMetadata();
    xrpl::STObject const noMetaObj = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    noMeta.transaction = noMetaObj.getSerializer().peekData();
    noMeta.ledgerSequence = kMaxSeq - 1;
    noMeta.date = 1;
    transactions.push_back(noMeta);

    auto complete = TransactionAndMetadata();
    xrpl::STObject const completeObj =
        createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    complete.transaction = completeObj.getSerializer().peekData();
    complete.ledgerSequence = kMinSeq + 1;
    xrpl::STObject const completeMeta =
        createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    complete.metadata = completeMeta.getSerializer().peekData();
    complete.date = 2;
    transactions.push_back(complete);

    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 1);
        auto const& tx = output.result->at("transactions").as_array()[0].as_object();
        EXPECT_EQ(tx.at("tx").as_object().at("ledger_index").as_uint64(), kMinSeq + 1);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, V2MissingLedgerHeaderOmitsCloseTimeAndLedgerHash)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(2);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        ASSERT_EQ(output.result->at("transactions").as_array().size(), 2);
        for (auto const& tx : output.result->at("transactions").as_array()) {
            auto const& obj = tx.as_object();
            EXPECT_FALSE(obj.contains("close_time_iso"));
            EXPECT_FALSE(obj.contains("ledger_hash"));
            EXPECT_TRUE(obj.contains("hash"));
            EXPECT_TRUE(obj.contains("tx_json"));
            EXPECT_TRUE(obj.contains("ledger_index"));
            EXPECT_TRUE(obj.at("validated").as_bool());
        }
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, ForwardTxBelowMinSeqNotClipped)
{
    // Forward traversal only clips above maxIndex; one below minIndex is kept, as account_tx does.
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, true, testing::Optional(testing::Eq(TransactionsCursor{kMinSeq + 2, 0})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index_min": {},
                    "ledger_index_max": {},
                    "forward": true
                }})JSON",
                kMptId,
                kMinSeq + 2,
                kMaxSeq
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("marker").as_object(),
            boost::json::parse(R"JSON({"ledger": 12, "seq": 34})JSON")
        );
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, MarkerSeedsCursorInForwardMode)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, _, true, testing::Optional(testing::Eq(TransactionsCursor{21, 22})), _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "forward": true,
                    "marker": {{"ledger": 21, "seq": 22}}
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, LimitAtMinIsPassedThrough)
{
    auto const transactions = genTransactions(kMinSeq + 1, kMaxSeq - 1);
    auto const transCursor =
        TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, static_cast<uint32_t>(MPTokenIssuanceHistoryHandler::kLimitMin), _, _, _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "limit": {}
                }})JSON",
                kMptId,
                MPTokenIssuanceHistoryHandler::kLimitMin
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("limit").as_uint64(), MPTokenIssuanceHistoryHandler::kLimitMin);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, DefaultLimitPassedToBackend)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchMPTokenIssuanceTransactions(
            _, static_cast<uint32_t>(MPTokenIssuanceHistoryHandler::kLimitDefault), _, _, _
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerTest, MptIssuanceIDIsNormalizedInOutput)
{
    auto const transCursor = TransactionsAndCursor{.txns = {}, .cursor = std::nullopt};
    ON_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillByDefault(Return(transCursor));

    EXPECT_CALL(*backend_, fetchMPTokenIssuanceTransactions(xrpl::uint192{kMptId}, _, _, _, _))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptIdLowercase)
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("mpt_issuance_id").as_string(), kMptId);
    });
}

TEST_F(RPCMPTokenIssuanceHistoryHandlerLogTest, LogsFetchDurationAndSkippedIndexEntry)
{
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj1 = createPaymentTransactionObject(kAccount, kAccount2, 1, 1, 32);
    trans1.transaction = obj1.getSerializer().peekData();
    trans1.ledgerSequence = kMinSeq + 1;
    xrpl::STObject const meta1 = createPaymentTransactionMetaObject(kAccount, kAccount2, 22, 23);
    trans1.metadata = meta1.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto emptyTrans = TransactionAndMetadata();
    emptyTrans.ledgerSequence = kMinSeq + 2;
    emptyTrans.date = 2;
    transactions.push_back(emptyTrans);

    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = std::nullopt};
    EXPECT_CALL(*backend_, fetchMPTokenIssuanceTransactions).WillOnce(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{MPTokenIssuanceHistoryHandler{backend_}};
        auto const req =
            boost::json::parse(fmt::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });

    auto const logs = getLoggerString();
    EXPECT_THAT(
        logs, ContainsRegex(R"(inf:RPC - db fetch took [0-9]+ milliseconds - num blobs = 2)")
    );
    EXPECT_THAT(
        logs,
        HasSubstr(
            fmt::format(
                "war:RPC - Skipping index entry with no matching transaction record; "
                "mpt_issuance_id = {}",
                kMptId
            )
        )
    );
}
