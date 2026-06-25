#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountNFTs.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kTokenId = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr auto kIssuer = "raSsG8F6KePke7sqw2MXYZ3mu7p68GvFma";
constexpr auto kSerial = 49386;
constexpr auto kTaxOn = 0;
constexpr auto kFlag = 8;
constexpr auto kTxnId = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kPage = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kInvalidPage = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FCAAA";
constexpr auto kMaxSeq = 30;
constexpr auto kMinSeq = 10;

}  // namespace

using namespace rpc;
using namespace data;
using namespace testing;

struct RPCAccountNFTsHandlerTest : HandlerBaseTest {
    RPCAccountNFTsHandlerTest()
    {
        backend_->setRange(kMinSeq, kMaxSeq);
    }
};

struct AccountNFTParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountNFTParameterTest : public RPCAccountNFTsHandlerTest,
                                 public WithParamInterface<AccountNFTParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountNFTParamTestCaseBundle>{
        {
            .testName = "AccountMissing",
            .testJson = R"JSON({})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing",
        },
        {
            .testName = "AccountNotString",
            .testJson = R"JSON({"account": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString",
        },
        {
            .testName = "AccountInvalid",
            .testJson = R"JSON({"account": "123"})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed",
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
            .testName = "LimitNotInt",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "LimitNegative",
            .testJson = R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": -1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "LimitZero",
            .testJson = R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 0})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "MarkerNotString",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerNotString",
        },
        {
            .testName = "MarkerInvalid",
            .testJson =
                R"JSON({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": "12;xxx"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerMalformed",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountNFTsGroup1,
    AccountNFTParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(AccountNFTParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaStringIndex)
{
    constexpr auto kSeq = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSeq, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "{}"
            }})JSON",
            kAccount,
            kSeq
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaIntIndex)
{
    constexpr auto kSeq = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSeq, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": {}
            }})JSON",
            kAccount,
            kSeq
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, AccountNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, NormalPath)
{
    static auto const kExpectedOutput = fmt::format(
        R"JSON({{
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true,
            "account": "{}",
            "account_nfts": [
                {{
                    "NFTokenID": "{}",
                    "URI": "7777772E6F6B2E636F6D",
                    "Flags": {},
                    "Issuer": "{}",
                    "NFTokenTaxon": {},
                    "nft_serial": {},
                    "TransferFee": 10000
                }}
            ],
            "limit": 100
        }})JSON",
        kLedgerHash,
        kAccount,
        kTokenId,
        kFlag,
        kIssuer,
        kTaxOn,
        kSerial
    );

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = xrpl::keylet::nftpageMax(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Limit)
{
    static constexpr auto kLimit = 20;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = xrpl::keylet::nftpageMax(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, firstPage
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1 + kLimit);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kAccount,
            kLimit
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 20);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), xrpl::strHex(firstPage));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Marker)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPage}, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{}"
            }})JSON",
            kAccount,
            kPage
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 1);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, InvalidMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{}"
            }})JSON",
            kAccount,
            kInvalidPage
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(
            err.at("error_message").as_string(), "Marker field does not match any valid Page ID"
        );
    });
}

TEST_F(RPCAccountNFTsHandlerTest, AccountWithNoNFT)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 0);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, invalidPage)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPage}, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{}"
            }})JSON",
            kAccount,
            kPage
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(
            err.at("error_message").as_string(), "Marker matches Page ID from another Account"
        );
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitLessThanMin)
{
    static auto const kExpectedOutput = fmt::format(
        R"JSON({{
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true,
            "account": "{}",
            "account_nfts": [
                {{
                    "NFTokenID": "{}",
                    "URI": "7777772E6F6B2E636F6D",
                    "Flags": {},
                    "Issuer": "{}",
                    "NFTokenTaxon": {},
                    "nft_serial": {},
                    "TransferFee": 10000
                }}
            ],
            "limit": {}
        }})JSON",
        kLedgerHash,
        kAccount,
        kTokenId,
        kFlag,
        kIssuer,
        kTaxOn,
        kSerial,
        AccountNFTsHandler::kLimitMin
    );

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = xrpl::keylet::nftpageMax(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kAccount,
            AccountNFTsHandler::kLimitMin - 1
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitMoreThanMax)
{
    static auto const kExpectedOutput = fmt::format(
        R"JSON({{
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true,
            "account": "{}",
            "account_nfts": [
                {{
                    "NFTokenID": "{}",
                    "URI": "7777772E6F6B2E636F6D",
                    "Flags": {},
                    "Issuer": "{}",
                    "NFTokenTaxon": {},
                    "nft_serial": {},
                    "TransferFee": 10000
                }}
            ],
            "limit": {}
        }})JSON",
        kLedgerHash,
        kAccount,
        kTokenId,
        kFlag,
        kIssuer,
        kTaxOn,
        kSerial,
        AccountNFTsHandler::kLimitMax
    );

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kMaxSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kAccount, 0, 1, 10, 2, kTxnId, 3);
    auto const accountID = getAccountIdWithString(kAccount);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = xrpl::keylet::nftpageMax(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kAccount,
            AccountNFTsHandler::kLimitMax + 1
        )
    );
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}
