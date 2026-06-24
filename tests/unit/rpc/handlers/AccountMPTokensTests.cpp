#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountMPTokens.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kIssuanceIdHex = "00080000B43A1A953EADDB3314A73523789947C752044C49";
constexpr auto kTokenIndeX1 = "A6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTokenIndeX2 = "B6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

constexpr uint64_t kTokeN1Amount = 500;
constexpr uint64_t kTokeN1LockedAmount = 50;
constexpr uint64_t kTokeN2Amount = 250;

// define expected JSON for mptokens
auto const kTokenOuT1 = fmt::format(
    R"JSON({{
        "mpt_id": "{}",
        "account": "{}",
        "mpt_issuance_id": "{}",
        "mpt_amount": {},
        "locked_amount": {},
        "mpt_locked": true
    }})JSON",
    kTokenIndeX1,
    kAccount,
    kIssuanceIdHex,
    kTokeN1Amount,
    kTokeN1LockedAmount
);

auto const kTokenOuT2 = fmt::format(
    R"JSON({{
        "mpt_id": "{}",
        "account": "{}",
        "mpt_issuance_id": "{}",
        "mpt_amount": {},
        "mpt_authorized": true
    }})JSON",
    kTokenIndeX2,
    kAccount,
    kIssuanceIdHex,
    kTokeN2Amount
);

}  // namespace

struct RPCAccountMPTokensHandlerTest : HandlerBaseTest {
    RPCAccountMPTokensHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

struct AccountMPTokensParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct AccountMPTokensParameterTest : RPCAccountMPTokensHandlerTest,
                                      WithParamInterface<AccountMPTokensParamTestCaseBundle> {};

// generate values for invalid params test
static auto
generateTestValuesForInvalidParamsTest()
{
    return std::vector<AccountMPTokensParamTestCaseBundle>{
        {.testName = "NonHexLedgerHash",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}", "ledger_hash": "xxx" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashMalformed"},
        {.testName = "NonStringLedgerHash",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_hash": 123 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashNotString"},
        {.testName = "InvalidLedgerIndexString",
         .testJson = fmt::format(
             R"JSON({{ "account": "{}", "ledger_index": "notvalidated" }})JSON", kAccount
         ),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledgerIndexMalformed"},
        {.testName = "MarkerNotString",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "marker": 9 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "markerNotString"},
        {.testName = "InvalidMarkerContent",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}", "marker": "123invalid" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Malformed cursor."},
        {.testName = "AccountMissing",
         .testJson = R"JSON({ "limit": 10 })JSON",
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Required field 'account' missing"},
        {.testName = "AccountNotString",
         .testJson = R"JSON({ "account": 123 })JSON",
         .expectedError = "actMalformed",
         .expectedErrorMessage = "Account malformed."},
        {.testName = "AccountMalformed",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}" }})JSON", "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"),
         .expectedError = "actMalformed",
         .expectedErrorMessage = "Account malformed."},
        {.testName = "LimitNotInteger",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": "t" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitNegative",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": -1 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitZero",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": 0 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitTypeInvalid",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": true }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."}
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokensInvalidParamsGroup,
    AccountMPTokensParameterTest,
    ValuesIn(generateTestValuesForInvalidParamsTest()),
    tests::util::kNameGenerator
);

// test invalid params bundle
TEST_P(AccountMPTokensParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash to return empty
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    // mock fetchLedgerBySequence to return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "4"
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    // mock fetchLedgerBySequence to return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": 4
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LedgerSeqOutOfRangeByHash)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 31);
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LedgerSeqOutOfRangeByIndex)
{
    // No need to check from db, call fetchLedgerBySequence 0 times
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "31"
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, NonExistAccount)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));
    // fetch account object return empty
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, DefaultParameters)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kTokenIndeX1}, xrpl::uint256{kTokenIndeX2}}, kTokenIndeX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN1Amount,
            xrpl::lsfMPTLocked,
            kTokeN1LockedAmount
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN2Amount,
            xrpl::lsfMPTAuthorized,
            std::nullopt
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const expected = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mptokens": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokensHandler::kLimitDefault,
            kTokenOuT1,
            kTokenOuT2
        );
        auto const input =
            boost::json::parse(fmt::format(R"JSON({{"account": "{}"}})JSON", kAccount));
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(expected), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, UseLimit)
{
    constexpr int kLimit = 20;
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const indexes = std::vector<xrpl::uint256>(50, xrpl::uint256{kTokenIndeX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(50);
        for (int i = 0; i < 50; ++i) {
            v.push_back(
                createMpTokenObject(kAccount, xrpl::uint192(kIssuanceIdHex), i, 0, std::nullopt)
                    .getSerializer()
                    .peekData()
            );
        }
        return v;
    }();

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kTokenIndeX1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this, kLimit](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                kLimit
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const resultJson = output.result->as_object();
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLimit);
        ASSERT_TRUE(resultJson.contains("marker"));
        EXPECT_THAT(boost::json::value_to<std::string>(resultJson.at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokensHandler::kLimitMin - 1
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(), AccountMPTokensHandler::kLimitMin
        );
    });

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokensHandler::kLimitMax + 1
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(), AccountMPTokensHandler::kLimitMax
        );
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, MarkerOutput)
{
    constexpr auto kNextPage = 99;
    constexpr auto kLimit = 15;
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const ownerDirKk = xrpl::keylet::ownerDir(account).key;
    auto const ownerDir2Kk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), kNextPage).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLimit);
        for (int i = 0; i < kLimit; ++i) {
            v.push_back(
                createMpTokenObject(kAccount, xrpl::uint192(kIssuanceIdHex), i, 0, std::nullopt)
                    .getSerializer()
                    .peekData()
            );
        }
        return v;
    }();
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    std::vector<xrpl::uint256> indexes1;
    indexes1.reserve(10);
    for (int i = 0; i < 10; ++i) {
        indexes1.emplace_back(kTokenIndeX1);
    }
    xrpl::STObject ownerDir1 = createOwnerDirLedgerObject(indexes1, kTokenIndeX1);
    ownerDir1.setFieldU64(xrpl::sfIndexNext, kNextPage);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir1.getSerializer().peekData()));

    xrpl::STObject ownerDir2 = createOwnerDirLedgerObject(indexes1, kTokenIndeX2);
    ownerDir2.setFieldU64(xrpl::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, _, _))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    runSpawn([this, kLimit, kNextPage](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                kLimit
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLimit);
        EXPECT_EQ(
            boost::json::value_to<std::string>(resultJson.at("marker")),
            fmt::format("{},{}", kTokenIndeX1, kNextPage)
        );
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, MarkerInput)
{
    constexpr auto kNextPage = 99;
    constexpr auto kLimit = 15;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    auto const ownerDirKk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), kNextPage).key;

    auto const indexes = std::vector<xrpl::uint256>(kLimit, xrpl::uint256{kTokenIndeX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLimit);
        for (int i = 0; i < kLimit; ++i) {
            v.push_back(
                createMpTokenObject(kAccount, xrpl::uint192(kIssuanceIdHex), i, 0, std::nullopt)
                    .getSerializer()
                    .peekData()
            );
        }
        return v;
    }();

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kTokenIndeX1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, kLimit, kNextPage](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {},
                    "marker": "{},{}"
                }})JSON",
                kAccount,
                kLimit,
                kTokenIndeX1,
                kNextPage
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        EXPECT_TRUE(resultJson.if_contains("marker") == nullptr);
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLimit - 1);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LimitLessThanMin)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kTokenIndeX1}, xrpl::uint256{kTokenIndeX2}}, kTokenIndeX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN1Amount,
            xrpl::lsfMPTLocked,
            kTokeN1LockedAmount
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN2Amount,
            xrpl::lsfMPTAuthorized,
            std::nullopt
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokensHandler::kLimitMin - 1
            )
        );

        auto const correctOutput = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mptokens": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokensHandler::kLimitMin,
            kTokenOuT1,
            kTokenOuT2
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LimitMoreThanMax)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kTokenIndeX1}, xrpl::uint256{kTokenIndeX2}}, kTokenIndeX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN1Amount,
            xrpl::lsfMPTLocked,
            kTokeN1LockedAmount
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kAccount,
            xrpl::uint192(kIssuanceIdHex),
            kTokeN2Amount,
            xrpl::lsfMPTAuthorized,
            std::nullopt
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokensHandler::kLimitMax + 1
            )
        );

        auto const correctOutput = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mptokens": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokensHandler::kLimitMax,
            kTokenOuT1,
            kTokenOuT2
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, EmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject({}, kTokenIndeX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kAccount
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("mptokens").as_array().size(), 0);
    });
}
