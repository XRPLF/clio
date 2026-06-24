#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AMMInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/UintTypes.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kSeq = 30;
constexpr auto kWrongAmmAccount = "000S7XL6nxRAi7JcbJcn1Na179oF300000";
constexpr auto kAmmAccount = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr auto kAmmAccounT2 = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr auto kLpIssueCurrency = "03930D02208264E2E40EC1B0C09E4DB96EE197B1";
constexpr auto kNotfoundAccount = "rBdLS7RVLqkPwnWQCT2bC6HJd6xGoBizq8";
constexpr auto kAmmId = 54321;
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

}  // namespace

struct RPCAMMInfoHandlerTest : HandlerBaseTest {
    RPCAMMInfoHandlerTest()
    {
        backend_->setRange(10, 30);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

struct AMMInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct AMMInfoParameterTest : RPCAMMInfoHandlerTest,
                              WithParamInterface<AMMInfoParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AMMInfoParamTestCaseBundle>{
        AMMInfoParamTestCaseBundle{
            .testName = "MissingAMMAccountOrAssets",
            .testJson = "{}",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAccountNotString",
            .testJson = R"JSON({"amm_account": 1})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AccountNotString",
            .testJson = R"JSON({"account": 1})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAccountInvalid",
            .testJson = R"JSON({"amm_account": "xxx"})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AccountInvalid",
            .testJson = R"JSON({"account": "xxx"})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAssetNotStringOrObject",
            .testJson = R"JSON({"asset": 1})JSON",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAssetEmptyObject",
            .testJson = R"JSON({"asset": {}})JSON",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAsset2NotStringOrObject",
            .testJson = R"JSON({"asset2": 1})JSON",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAsset2EmptyObject",
            .testJson = R"JSON({"asset2": {}})JSON",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAMMInfoGroup1,
    AMMInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(AMMInfoParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAMMInfoHandlerTest, AccountNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    auto const missingAccountKey = getAccountKey(kNotfoundAccount);
    auto const accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const accountKey = getAccountKey(kAmmAccount);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(missingAccountKey, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}",
                "account": "{}"
            }})JSON",
            kAmmAccount,
            kNotfoundAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotExist)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kWrongAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotInDBIsMalformed)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotFoundMissingAmmField)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    auto const accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAmmBlobNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    auto const accountKey = getAccountKey(kAmmAccount);
    auto const ammId = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammId);

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccounT2, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "JPY", kAmmAccounT2
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, xrpl::uint256{kAmmId});

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAccBlobNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, 30);
    auto const accountKey = getAccountKey(kAmmAccount);
    auto const account2Key = getAccountKey(kAmmAccounT2);
    auto const ammId = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammId);

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const ammObj = createAmmObject(
        kAmmAccounT2, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "JPY", kAmmAccounT2
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammId);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(account2Key, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathMinimalFirstXRPNoTrustline)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccount,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        "JPY",
        kAmmAccounT2,
        kLpIssueCurrency
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": "193",
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAccount)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account2, account1, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const account2Root = createAccountRootObject(kAmmAccounT2, 0, 2, 300, 2, kIndex1, 2);
    auto const ammObj = createAmmObject(
        kAmmAccounT2,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        "JPY",
        kAmmAccount,
        kLpIssueCurrency
    );
    auto const lptCurrency = createLptCurrency("XRP", "JPY");
    auto const accountHoldsKeylet = xrpl::keylet::line(account2, account2, lptCurrency);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustline = createRippleStateLedgerObject(
        kLpIssueCurrency, kAmmAccount, 12, kAmmAccounT2, 1000, kAmmAccount, 2000, kIndex1, 2
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(std::optional<Blob>{}));
    ON_CALL(*backend_, doFetchLedgerObject(accountHoldsKeylet.key, kSeq, _))
        .WillByDefault(Return(trustline.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}",
                "account": "{}"
            }})JSON",
            kAmmAccount,
            kAmmAccounT2
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto const expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "12"
                        }},
                        "amount": "293",
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccounT2,
                "JPY",
                kAmmAccount,
                kAmmAccounT2,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathMinimalSecondXRPNoTrustline)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccount,
        "JPY",
        kAmmAccounT2,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        kLpIssueCurrency
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto const expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "amount2": "193",
                        "account": "{}",
                        "trading_fee": 5,
                        "asset_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathNonXRPNoTrustlines)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj =
        createAmmObject(kAmmAccount, "USD", kAmmAccount, "JPY", kAmmAccounT2, kLpIssueCurrency);
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto const expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset_frozen": false,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "USD",
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathFrozen)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue1LineKey = xrpl::keylet::line(account1, account1, xrpl::toCurrency("USD")).key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj =
        createAmmObject(kAmmAccount, "USD", kAmmAccount, "JPY", kAmmAccounT2, kLpIssueCurrency);
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    // note: frozen flag will not be used for trustline1 because issuer == account
    auto const trustline1BalanceFrozen = createRippleStateLedgerObject(
        "USD",
        kAmmAccount,
        8,
        kAmmAccount,
        1000,
        kAmmAccounT2,
        2000,
        kIndex1,
        2,
        xrpl::lsfGlobalFreeze
    );
    auto const trustline2BalanceFrozen = createRippleStateLedgerObject(
        "JPY",
        kAmmAccount,
        12,
        kAmmAccounT2,
        1000,
        kAmmAccount,
        2000,
        kIndex1,
        2,
        xrpl::lsfGlobalFreeze
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue1LineKey, kSeq, _))
        .WillByDefault(Return(trustline1BalanceFrozen.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(trustline2BalanceFrozen.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto const expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "8"
                        }},
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "-12"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset_frozen": false,
                        "asset2_frozen": true
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "USD",
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathFrozenIssuer)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue1LineKey = xrpl::keylet::line(account1, account1, xrpl::toCurrency("USD")).key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    // asset1 will be frozen because flag set here
    auto accountRoot =
        createAccountRootObject(kAmmAccount, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2);
    auto ammObj =
        createAmmObject(kAmmAccount, "USD", kAmmAccount, "JPY", kAmmAccounT2, kLpIssueCurrency);
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    // note: frozen flag will not be used for trustline1 because issuer == account
    auto const trustline1BalanceFrozen = createRippleStateLedgerObject(
        "USD",
        kAmmAccount,
        8,
        kAmmAccount,
        1000,
        kAmmAccounT2,
        2000,
        kIndex1,
        2,
        xrpl::lsfGlobalFreeze
    );
    auto const trustline2BalanceFrozen = createRippleStateLedgerObject(
        "JPY",
        kAmmAccount,
        12,
        kAmmAccounT2,
        1000,
        kAmmAccount,
        2000,
        kIndex1,
        2,
        xrpl::lsfGlobalFreeze
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue1LineKey, kSeq, _))
        .WillByDefault(Return(trustline1BalanceFrozen.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(trustline2BalanceFrozen.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto const expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "8"
                        }},
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "-12"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset_frozen": true,
                        "asset2_frozen": true
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "USD",
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithTrustline)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccount,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        "JPY",
        kAmmAccounT2,
        kLpIssueCurrency
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance = createRippleStateLedgerObject(
        "JPY", kAmmAccounT2, -8, kAmmAccount, 1000, kAmmAccounT2, 2000, kIndex2, 2, 0
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": "193",
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "8"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithVoteSlots)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccount,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        "JPY",
        kAmmAccounT2,
        kLpIssueCurrency
    );
    ammAddVoteSlot(ammObj, account1, 2, 4);
    ammAddVoteSlot(ammObj, account2, 4, 2);
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance = createRippleStateLedgerObject(
        "JPY", kAmmAccounT2, -8, kAmmAccount, 1000, kAmmAccounT2, 2000, kIndex2, 2, 0
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": "193",
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "8"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "vote_slots": [
                            {{
                                "account": "{}",
                                "trading_fee": 2,
                                "vote_weight": 4
                            }},
                            {{
                                "account": "{}",
                                "trading_fee": 4,
                                "vote_weight": 2
                            }}
                        ],
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kAmmAccount,
                kAmmAccounT2,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAuctionSlot)
{
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const ammKey = xrpl::uint256{kAmmId};
    auto const ammKeylet = xrpl::keylet::amm(ammKey);
    auto const feesKey = xrpl::keylet::fees().key;
    auto const issue2LineKey = xrpl::keylet::line(account1, account2, xrpl::toCurrency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj = createAmmObject(
        kAmmAccount,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        "JPY",
        kAmmAccounT2,
        kLpIssueCurrency
    );
    ammSetAuctionSlot(
        ammObj,
        account2,
        xrpl::amountFromString(xrpl::xrpIssue(), "100"),
        2,
        25 * 3600,
        {account1, account2}
    );

    accountRoot.setFieldH256(xrpl::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance = createRippleStateLedgerObject(
        "JPY", kAmmAccounT2, -8, kAmmAccount, 1000, kAmmAccounT2, 2000, kIndex2, 2, 0
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSeq, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSeq, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "amm_account": "{}"
            }})JSON",
            kAmmAccount
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": "193",
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "8"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "auction_slot": {{
                            "time_interval": 20,
                            "price": "100",
                            "discounted_fee": 2,
                            "account": "{}",
                            "expiration": "2000-01-02T01:00:00+0000",
                            "auth_accounts": [
                                {{
                                    "account": "{}"
                                }},
                                {{
                                    "account": "{}"
                                }}
                            ]
                        }},
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kAmmAccounT2,
                kAmmAccount,
                kAmmAccounT2,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAssetsMatchingInputOrder)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const issue1 = xrpl::Issue(xrpl::toCurrency("JPY"), account1);
    auto const issue2 = xrpl::Issue(xrpl::toCurrency("USD"), account2);
    auto const ammKeylet = xrpl::keylet::amm(issue1, issue2);

    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto ammObj =
        createAmmObject(kAmmAccount, "JPY", kAmmAccount, "USD", kAmmAccounT2, kLpIssueCurrency);
    auto const auctionIssue = xrpl::Issue{xrpl::Currency{kLpIssueCurrency}, account1};
    ammSetAuctionSlot(
        ammObj,
        account2,
        xrpl::amountFromString(auctionIssue, "100"),
        2,
        25 * 3600,
        {account1, account2}
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKeylet.key);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "asset": {{
                    "currency": "JPY",
                    "issuer": "{}"
                }},
                "asset2": {{
                    "currency": "USD",
                    "issuer": "{}"
                }}
            }})JSON",
            kAmmAccount,
            kAmmAccounT2
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "auction_slot": {{
                            "time_interval": 20,
                            "price": {{
                                "currency": "{}",
                                "issuer": "{}",
                                "value": "100"
                            }},
                            "discounted_fee": 2,
                            "account": "{}",
                            "expiration": "2000-01-02T01:00:00+0000",
                            "auth_accounts": [
                                {{
                                    "account": "{}"
                                }},
                                {{
                                    "account": "{}"
                                }}
                            ]
                        }},
                        "asset_frozen": false,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "JPY",
                kAmmAccount,
                "USD",
                kAmmAccounT2,
                kAmmAccount,
                kLpIssueCurrency,
                kAmmAccount,
                kAmmAccounT2,
                kAmmAccount,
                kAmmAccounT2,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAssetsPreservesInputOrder)
{
    auto const lgrInfo = createLedgerHeader(kLedgerHash, kSeq);
    auto const account1 = getAccountIdWithString(kAmmAccount);
    auto const account2 = getAccountIdWithString(kAmmAccounT2);
    auto const issue1 = xrpl::Issue(xrpl::toCurrency("USD"), account1);
    auto const issue2 = xrpl::Issue(xrpl::toCurrency("JPY"), account2);
    auto const ammKeylet = xrpl::keylet::amm(issue1, issue2);

    // Note: order in the AMM object is different from the input
    auto ammObj =
        createAmmObject(kAmmAccount, "JPY", kAmmAccount, "USD", kAmmAccounT2, kLpIssueCurrency);
    auto accountRoot = createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const auctionIssue = xrpl::Issue{xrpl::Currency{kLpIssueCurrency}, account1};
    ammSetAuctionSlot(
        ammObj,
        account2,
        xrpl::amountFromString(auctionIssue, "100"),
        2,
        25 * 3600,
        {account1, account2}
    );
    accountRoot.setFieldH256(xrpl::sfAMMID, ammKeylet.key);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "asset": {{
                    "currency": "USD",
                    "issuer": "{}"
                }},
                "asset2": {{
                    "currency": "JPY",
                    "issuer": "{}"
                }}
            }})JSON",
            kAmmAccount,
            kAmmAccounT2
        )
    );

    auto const handler = AnyHandler{AMMInfoHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        auto expectedResult = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "amm": {{
                        "lp_token": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "amount": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "amount2": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "0"
                        }},
                        "account": "{}",
                        "trading_fee": 5,
                        "auction_slot": {{
                            "time_interval": 20,
                            "price": {{
                                "currency": "{}",
                                "issuer": "{}",
                                "value": "100"
                            }},
                            "discounted_fee": 2,
                            "account": "{}",
                            "expiration": "2000-01-02T01:00:00+0000",
                            "auth_accounts": [
                                {{
                                    "account": "{}"
                                }},
                                {{
                                    "account": "{}"
                                }}
                            ]
                        }},
                        "asset_frozen": false,
                        "asset2_frozen": false
                    }},
                    "ledger_index": 30,
                    "ledger_hash": "{}",
                    "validated": true
                }})JSON",
                kLpIssueCurrency,
                kAmmAccount,
                "USD",
                kAmmAccount,
                "JPY",
                kAmmAccounT2,
                kAmmAccount,
                kLpIssueCurrency,
                kAmmAccount,
                kAmmAccounT2,
                kAmmAccount,
                kAmmAccounT2,
                kLedgerHash
            )
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}
