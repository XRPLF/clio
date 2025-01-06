//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include "rpc/handlers/AccountNFTs.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
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

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kTOKEN_ID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr auto kISSUER = "raSsG8F6KePke7sqw2MXYZ3mu7p68GvFma";
constexpr auto kSERIAL = 49386;
constexpr auto kTAX_ON = 0;
constexpr auto kFLAG = 8;
constexpr auto kTXN_ID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kPAGE = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kINVALID_PAGE = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FCAAA";
constexpr auto kMAX_SEQ = 30;
constexpr auto kMIN_SEQ = 10;

}  // namespace

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct RPCAccountNFTsHandlerTest : HandlerBaseTest {
    RPCAccountNFTsHandlerTest()
    {
        backend_->setRange(kMIN_SEQ, kMAX_SEQ);
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
            .testJson = R"({})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing",
        },
        {
            .testName = "AccountNotString",
            .testJson = R"({"account": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString",
        },
        {
            .testName = "AccountInvalid",
            .testJson = R"({"account": "123"})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed",
        },
        {
            .testName = "LedgerHashInvalid",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed",
        },
        {
            .testName = "LedgerHashNotString",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString",
        },
        {
            .testName = "LedgerIndexNotInt",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed",
        },
        {
            .testName = "LimitNotInt",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "LimitNegative",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": -1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "LimitZero",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 0})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "MarkerNotString",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerNotString",
        },
        {
            .testName = "MarkerInvalid",
            .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": "12;xxx"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerMalformed",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountNFTsGroup1,
    AccountNFTParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountNFTParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
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
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaStringIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"{}"
        }})",
        kACCOUNT,
        kSEQ
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaIntIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":{}
        }})",
        kACCOUNT,
        kSEQ
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, AccountNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, NormalPath)
{
    static auto const kEXPECTED_OUTPUT = fmt::format(
        R"({{
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true,
            "account":"{}",
            "account_nfts":[
                {{
                    "NFTokenID":"{}",
                    "URI":"7777772E6F6B2E636F6D",
                    "Flags":{},
                    "Issuer":"{}",
                    "NFTokenTaxon":{},
                    "nft_serial":{},
                    "TransferFee":10000
                }}
            ],
            "limit":100
        }})",
        kLEDGER_HASH,
        kACCOUNT,
        kTOKEN_ID,
        kFLAG,
        kISSUER,
        kTAX_ON,
        kSERIAL
    );

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Limit)
{
    static constexpr auto kLIMIT = 20;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject =
        createNftTokenPage(std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, firstPage);
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1 + kLIMIT);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        kLIMIT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 20);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), ripple::strHex(firstPage));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Marker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kPAGE}, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{}"
        }})",
        kACCOUNT,
        kPAGE
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 1);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, InvalidMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{}"
        }})",
        kACCOUNT,
        kINVALID_PAGE
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Marker field does not match any valid Page ID");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, AccountWithNoNFT)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 0);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, invalidPage)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kPAGE}, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{}"
        }})",
        kACCOUNT,
        kPAGE
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Marker matches Page ID from another Account");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitLessThanMin)
{
    static auto const kEXPECTED_OUTPUT = fmt::format(
        R"({{
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true,
            "account":"{}",
            "account_nfts":[
                {{
                    "NFTokenID":"{}",
                    "URI":"7777772E6F6B2E636F6D",
                    "Flags":{},
                    "Issuer":"{}",
                    "NFTokenTaxon":{},
                    "nft_serial":{},
                    "TransferFee":10000
                }}
            ],
            "limit":{}
        }})",
        kLEDGER_HASH,
        kACCOUNT,
        kTOKEN_ID,
        kFLAG,
        kISSUER,
        kTAX_ON,
        kSERIAL,
        AccountNFTsHandler::kLIMIT_MIN
    );

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        AccountNFTsHandler::kLIMIT_MIN - 1
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitMoreThanMax)
{
    static auto const kEXPECTED_OUTPUT = fmt::format(
        R"({{
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true,
            "account":"{}",
            "account_nfts":[
                {{
                    "NFTokenID":"{}",
                    "URI":"7777772E6F6B2E636F6D",
                    "Flags":{},
                    "Issuer":"{}",
                    "NFTokenTaxon":{},
                    "nft_serial":{},
                    "TransferFee":10000
                }}
            ],
            "limit":{}
        }})",
        kLEDGER_HASH,
        kACCOUNT,
        kTOKEN_ID,
        kFLAG,
        kISSUER,
        kTAX_ON,
        kSERIAL,
        AccountNFTsHandler::kLIMIT_MAX
    );

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = createAccountRootObject(kACCOUNT, 0, 1, 10, 2, kTXN_ID, 3);
    auto const accountID = getAccountIdWithString(kACCOUNT);
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        AccountNFTsHandler::kLIMIT_MAX + 1
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}
