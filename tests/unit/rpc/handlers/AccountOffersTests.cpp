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

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

}  // namespace

using namespace rpc;
using namespace data;
namespace json = boost::json;
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
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountOfferParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountOffersHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
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
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaStringIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "{}"
            }})JSON",
            kACCOUNT,
            kSEQ
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaIntIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": {}
            }})JSON",
            kACCOUNT,
            kSEQ
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, AccountNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kACCOUNT
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
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
        kLEDGER_HASH,
        kACCOUNT,
        kACCOUNT2
    );
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );
    offer.setFieldU32(ripple::sfExpiration, 123);
    bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kACCOUNT
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Limit)
{
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{20, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++) {
        auto const offer = createOfferLedgerObject(
            kACCOUNT,
            10,
            20,
            ripple::to_string(ripple::to_currency("USD")),
            ripple::to_string(ripple::xrpCurrency()),
            kACCOUNT2,
            toBase58(ripple::xrpAccount()),
            kINDEX1
        );
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": 10
            }})JSON",
            kACCOUNT
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 10);
        EXPECT_EQ(output.result->at("marker").as_string(), fmt::format("{},0", kINDEX1));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Marker)
{
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{20, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*backend_, doFetchLedgerObject(hintIndex, kLEDGER_SEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++) {
        auto const offer = createOfferLedgerObject(
            kACCOUNT,
            10,
            20,
            ripple::to_string(ripple::to_currency("USD")),
            ripple::to_string(ripple::xrpCurrency()),
            kACCOUNT2,
            toBase58(ripple::xrpAccount()),
            kINDEX1
        );
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{},{}"
            }})JSON",
            kACCOUNT,
            kINDEX1,
            startPage
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 19);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountOffersHandlerTest, MarkerNotExists)
{
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*backend_, doFetchLedgerObject(hintIndex, kLEDGER_SEQ, _))
        .WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "marker": "{},{}"
            }})JSON",
            kACCOUNT,
            kINDEX1,
            startPage
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitLessThanMin)
{
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{AccountOffersHandler::kLIMIT_MIN + 1, ripple::uint256{kINDEX1}}, kINDEX1
    );
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );
    offer.setFieldU32(ripple::sfExpiration, 123);

    bbs.reserve(AccountOffersHandler::kLIMIT_MIN + 1);
    for (auto i = 0; i < AccountOffersHandler::kLIMIT_MIN + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kACCOUNT,
            AccountOffersHandler::kLIMIT_MIN - 1
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), AccountOffersHandler::kLIMIT_MIN);
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitMoreThanMax)
{
    constexpr auto kLEDGER_SEQ = 30;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kLEDGER_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{AccountOffersHandler::kLIMIT_MAX + 1, ripple::uint256{kINDEX1}}, kINDEX1
    );

    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kLEDGER_SEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );
    offer.setFieldU32(ripple::sfExpiration, 123);
    bbs.reserve(AccountOffersHandler::kLIMIT_MAX + 1);
    for (auto i = 0; i < AccountOffersHandler::kLIMIT_MAX + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kINPUT = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "limit": {}
            }})JSON",
            kACCOUNT,
            AccountOffersHandler::kLIMIT_MAX + 1
        )
    );
    auto const handler = AnyHandler{AccountOffersHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), AccountOffersHandler::kLIMIT_MAX);
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
