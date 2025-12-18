//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kISSUANCE_ID_HEX = "00080000B43A1A953EADDB3314A73523789947C752044C49";
constexpr auto kTOKEN_INDEX1 = "A6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTOKEN_INDEX2 = "B6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

constexpr uint64_t kTOKEN1_AMOUNT = 500;
constexpr uint64_t kTOKEN1_LOCKED_AMOUNT = 50;
constexpr uint64_t kTOKEN2_AMOUNT = 250;

// define expected JSON for mptokens
auto const kTOKEN_OUT1 = fmt::format(
    R"JSON({{
        "mpt_id": "{}",
        "account": "{}",
        "mpt_issuance_id": "{}",
        "mpt_amount": {},
        "locked_amount": {},
        "mpt_locked": true
    }})JSON",
    kTOKEN_INDEX1,
    kACCOUNT,
    kISSUANCE_ID_HEX,
    kTOKEN1_AMOUNT,
    kTOKEN1_LOCKED_AMOUNT
);

auto const kTOKEN_OUT2 = fmt::format(
    R"JSON({{
        "mpt_id": "{}",
        "account": "{}",
        "mpt_issuance_id": "{}",
        "mpt_amount": {},
        "mpt_authorized": true
    }})JSON",
    kTOKEN_INDEX2,
    kACCOUNT,
    kISSUANCE_ID_HEX,
    kTOKEN2_AMOUNT
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
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_hash": "xxx" }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashMalformed"},
        {.testName = "NonStringLedgerHash",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_hash": 123 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashNotString"},
        {.testName = "InvalidLedgerIndexString",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_index": "notvalidated" }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledgerIndexMalformed"},
        {.testName = "MarkerNotString",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "marker": 9 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "markerNotString"},
        {.testName = "InvalidMarkerContent",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "marker": "123invalid" }})JSON", kACCOUNT),
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
         .testJson = fmt::format(R"JSON({{ "account": "{}" }})JSON", "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"),
         .expectedError = "actMalformed",
         .expectedErrorMessage = "Account malformed."},
        {.testName = "LimitNotInteger",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": "t" }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitNegative",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": -1 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitZero",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": 0 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitTypeInvalid",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": true }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."}
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokensInvalidParamsGroup,
    AccountMPTokensParameterTest,
    ValuesIn(generateTestValuesForInvalidParamsTest()),
    tests::util::kNAME_GENERATOR
);

// test invalid params bundle
TEST_P(AccountMPTokensParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokensHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
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
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "4"
            }})JSON",
            kACCOUNT
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": 4
            }})JSON",
            kACCOUNT
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 31);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "31"
            }})JSON",
            kACCOUNT
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));
    // fetch account object return empty
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kTOKEN_INDEX1}, ripple::uint256{kTOKEN_INDEX2}}, kTOKEN_INDEX1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN1_AMOUNT, ripple::lsfMPTLocked, kTOKEN1_LOCKED_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN2_AMOUNT, ripple::lsfMPTAuthorized, std::nullopt
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokensHandler::kLIMIT_DEFAULT,
            kTOKEN_OUT1,
            kTOKEN_OUT2
        );
        auto const input = json::parse(fmt::format(R"JSON({{"account": "{}"}})JSON", kACCOUNT));
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(expected), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, UseLimit)
{
    constexpr int kLIMIT = 20;
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const indexes = std::vector<ripple::uint256>(50, ripple::uint256{kTOKEN_INDEX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(50);
        for (int i = 0; i < 50; ++i) {
            v.push_back(createMpTokenObject(kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), i, 0, std::nullopt)
                            .getSerializer()
                            .peekData());
        }
        return v;
    }();

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kTOKEN_INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this, kLIMIT](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                kLIMIT
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const resultJson = (*output.result).as_object();
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLIMIT);
        ASSERT_TRUE(resultJson.contains("marker"));
        EXPECT_THAT(boost::json::value_to<std::string>(resultJson.at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokensHandler::kLIMIT_MIN - 1
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountMPTokensHandler::kLIMIT_MIN);
    });

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokensHandler::kLIMIT_MAX + 1
            )
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountMPTokensHandler::kLIMIT_MAX);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, MarkerOutput)
{
    constexpr auto kNEXT_PAGE = 99;
    constexpr auto kLIMIT = 15;
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    auto const ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLIMIT);
        for (int i = 0; i < kLIMIT; ++i) {
            v.push_back(createMpTokenObject(kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), i, 0, std::nullopt)
                            .getSerializer()
                            .peekData());
        }
        return v;
    }();
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    std::vector<ripple::uint256> indexes1;
    indexes1.reserve(10);
    for (int i = 0; i < 10; ++i) {
        indexes1.emplace_back(kTOKEN_INDEX1);
    }
    ripple::STObject ownerDir1 = createOwnerDirLedgerObject(indexes1, kTOKEN_INDEX1);
    ownerDir1.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir1.getSerializer().peekData()));

    ripple::STObject ownerDir2 = createOwnerDirLedgerObject(indexes1, kTOKEN_INDEX2);
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, _, _))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    runSpawn([this, kLIMIT, kNEXT_PAGE](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                kLIMIT
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = (*output.result).as_object();
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLIMIT);
        EXPECT_EQ(
            boost::json::value_to<std::string>(resultJson.at("marker")), fmt::format("{},{}", kTOKEN_INDEX1, kNEXT_PAGE)
        );
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, MarkerInput)
{
    constexpr auto kNEXT_PAGE = 99;
    constexpr auto kLIMIT = 15;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    auto const ownerDirKk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;

    auto const indexes = std::vector<ripple::uint256>(kLIMIT, ripple::uint256{kTOKEN_INDEX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLIMIT);
        for (int i = 0; i < kLIMIT; ++i) {
            v.push_back(createMpTokenObject(kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), i, 0, std::nullopt)
                            .getSerializer()
                            .peekData());
        }
        return v;
    }();

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kTOKEN_INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, kLIMIT, kNEXT_PAGE](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {},
                    "marker": "{},{}"
                }})JSON",
                kACCOUNT,
                kLIMIT,
                kTOKEN_INDEX1,
                kNEXT_PAGE
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = (*output.result).as_object();
        EXPECT_TRUE(resultJson.if_contains("marker") == nullptr);
        EXPECT_EQ(resultJson.at("mptokens").as_array().size(), kLIMIT - 1);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LimitLessThanMin)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kTOKEN_INDEX1}, ripple::uint256{kTOKEN_INDEX2}}, kTOKEN_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN1_AMOUNT, ripple::lsfMPTLocked, kTOKEN1_LOCKED_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN2_AMOUNT, ripple::lsfMPTAuthorized, std::nullopt
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokensHandler::kLIMIT_MIN - 1
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokensHandler::kLIMIT_MIN,
            kTOKEN_OUT1,
            kTOKEN_OUT2
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, LimitMoreThanMax)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kTOKEN_INDEX1}, ripple::uint256{kTOKEN_INDEX2}}, kTOKEN_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN1_AMOUNT, ripple::lsfMPTLocked, kTOKEN1_LOCKED_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMpTokenObject(
            kACCOUNT, ripple::uint192(kISSUANCE_ID_HEX), kTOKEN2_AMOUNT, ripple::lsfMPTAuthorized, std::nullopt
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokensHandler::kLIMIT_MAX + 1
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokensHandler::kLIMIT_MAX,
            kTOKEN_OUT1,
            kTOKEN_OUT2
        );

        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokensHandlerTest, EmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject({}, kTOKEN_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
            )
        );
        auto const handler = AnyHandler{AccountMPTokensHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("mptokens").as_array().size(), 0);
    });
}
