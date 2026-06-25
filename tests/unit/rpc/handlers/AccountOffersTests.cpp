#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountOffers.hpp"
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
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

}  // namespace

using namespace rpc;
using namespace data;
using namespace testing;

struct RPCAccountOffersHandlerTest : HandlerBaseTest {
    RPCAccountOffersHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

struct AccountOfferParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountOfferParameterTest : public RPCAccountOffersHandlerTest,
                                   public WithParamInterface<AccountOfferParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountOfferParamTestCaseBundle>{
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
            .expectedErrorMessage = "Malformed cursor.",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountOffersGroup1,
    AccountOfferParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(AccountOfferParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountOffersHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaHash)
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
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaStringIndex)
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
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaIntIndex)
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
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, AccountNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
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
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountOffersHandlerTest, DefaultParams)
{
    auto const expectedOutput = fmt::format(
        R"JSON({{
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true,
            "account": "{}",
            "offers": [
                {{
                    "seq": 0,
                    "flags": 0,
                    "quality": "0.000000024999999374023",
                    "taker_pays": "20",
                    "taker_gets": {{
                        "currency": "USD",
                        "issuer": "{}",
                        "value": "10"
                    }},
                    "expiration": 123
                }}
            ]
        }})JSON",
        kLedgerHash,
        kAccount,
        kAccount2
    );
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex1}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLedgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount2,
        toBase58(xrpl::xrpAccount()),
        kIndex1
    );
    offer.setFieldU32(xrpl::sfExpiration, 123);
    bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Limit)
{
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{20, xrpl::uint256{kIndex1}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLedgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++) {
        auto const offer = createOfferLedgerObject(
            kAccount,
            10,
            20,
            xrpl::to_string(xrpl::toCurrency("USD")),
            xrpl::to_string(xrpl::xrpCurrency()),
            kAccount2,
            toBase58(xrpl::xrpAccount()),
            kIndex1
        );
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": 10
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 10);
        EXPECT_EQ(output.result->at("marker").as_string(), fmt::format("{},0", kIndex1));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Marker)
{
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{20, xrpl::uint256{kIndex1}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    auto const hintIndex = xrpl::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*backend_, doFetchLedgerObject(hintIndex, kLedgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++) {
        auto const offer = createOfferLedgerObject(
            kAccount,
            10,
            20,
            xrpl::to_string(xrpl::toCurrency("USD")),
            xrpl::to_string(xrpl::xrpCurrency()),
            kAccount2,
            toBase58(xrpl::xrpAccount()),
            kIndex1
        );
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{},{}"
            }})JSON",
            kAccount,
            kIndex1,
            startPage
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 19);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountOffersHandlerTest, MarkerNotExists)
{
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    auto const hintIndex = xrpl::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*backend_, doFetchLedgerObject(hintIndex, kLedgerSeq, _))
        .WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{},{}"
            }})JSON",
            kAccount,
            kIndex1,
            startPage
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitLessThanMin)
{
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{AccountOffersHandler::kLimitMin + 1, xrpl::uint256{kIndex1}}, kIndex1
    );
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLedgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount2,
        toBase58(xrpl::xrpAccount()),
        kIndex1
    );
    offer.setFieldU32(xrpl::sfExpiration, 123);

    bbs.reserve(AccountOffersHandler::kLimitMin + 1);
    for (auto i = 0; i < AccountOffersHandler::kLimitMin + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kAccount,
            AccountOffersHandler::kLimitMin - 1
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), AccountOffersHandler::kLimitMin);
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitMoreThanMax)
{
    constexpr auto kLedgerSeq = 30;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kLedgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLedgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{AccountOffersHandler::kLimitMax + 1, xrpl::uint256{kIndex1}}, kIndex1
    );

    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLedgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount2,
        toBase58(xrpl::xrpAccount()),
        kIndex1
    );
    offer.setFieldU32(xrpl::sfExpiration, 123);
    bbs.reserve(AccountOffersHandler::kLimitMax + 1);
    for (auto i = 0; i < AccountOffersHandler::kLimitMax + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kAccount,
            AccountOffersHandler::kLimitMax + 1
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), AccountOffersHandler::kLimitMax);
    });
}

TEST(RPCAccountOffersHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"account", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
        {"ledger_hash", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"ledger_index", 30},
        {"marker", "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun,0"},
        {"limit", 200},
        {"ledger", 123},
        {"strict", true},
    };
    auto const spec = AccountOffersHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(
        warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated)
    );
    for (auto const& field : {"ledger", "strict"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)),
            std::string::npos
        ) << warning;
    }
}
