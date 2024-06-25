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
#include "util/Fixtures.hpp"
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

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto TOKENID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr static auto ISSUER = "raSsG8F6KePke7sqw2MXYZ3mu7p68GvFma";
constexpr static auto SERIAL = 49386;
constexpr static auto TAXON = 0;
constexpr static auto FLAG = 8;
constexpr static auto TXNID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto PAGE = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto MAXSEQ = 30;
constexpr static auto MINSEQ = 10;

using namespace rpc;
namespace json = boost::json;
using namespace testing;

class RPCAccountNFTsHandlerTest : public HandlerBaseTest {};

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
            "AccountMissing",
            R"({})",
            "invalidParams",
            "Required field 'account' missing",
        },
        {
            "AccountNotString",
            R"({"account": 123})",
            "invalidParams",
            "accountNotString",
        },
        {
            "AccountInvalid",
            R"({"account": "123"})",
            "actMalformed",
            "accountMalformed",
        },
        {
            "LedgerHashInvalid",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})",
            "invalidParams",
            "ledger_hashMalformed",
        },
        {
            "LedgerHashNotString",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})",
            "invalidParams",
            "ledger_hashNotString",
        },
        {
            "LedgerIndexNotInt",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})",
            "invalidParams",
            "ledgerIndexMalformed",
        },
        {
            "LimitNotInt",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "x"})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "LimitNegative",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": -1})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "LimitZero",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 0})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "MarkerNotString",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": 123})",
            "invalidParams",
            "markerNotString",
        },
        {
            "MarkerInvalid",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": "12;xxx"})",
            "invalidParams",
            "markerMalformed",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountNFTsGroup1,
    AccountNFTParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(AccountNFTParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountNFTsHandler{backend}};
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
    backend->setRange(MINSEQ, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaStringIndex)
{
    auto constexpr seq = 12;

    backend->setRange(MINSEQ, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"{}"
        }})",
        ACCOUNT,
        seq
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LedgerNotFoundViaIntIndex)
{
    auto constexpr seq = 12;

    backend->setRange(MINSEQ, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":{}
        }})",
        ACCOUNT,
        seq
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, AccountNotFound)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountNFTsHandlerTest, NormalPath)
{
    static auto const expectedOutput = fmt::format(
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
        LEDGERHASH,
        ACCOUNT,
        TOKENID,
        FLAG,
        ISSUER,
        TAXON,
        SERIAL
    );

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = CreateAccountRootObject(ACCOUNT, 0, 1, 10, 2, TXNID, 3);
    auto const accountID = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    ON_CALL(*backend, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Limit)
{
    static auto constexpr limit = 20;

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = CreateAccountRootObject(ACCOUNT, 0, 1, 10, 2, TXNID, 3);
    auto const accountID = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, firstPage);
    ON_CALL(*backend, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1 + limit);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        limit
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 20);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), ripple::strHex(firstPage));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, Marker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = CreateAccountRootObject(ACCOUNT, 0, 1, 10, 2, TXNID, 3);
    auto const accountID = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const pageObject =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    ON_CALL(*backend, doFetchLedgerObject(ripple::uint256{PAGE}, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{}"
        }})",
        ACCOUNT,
        PAGE
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_nfts").as_array().size(), 1);
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitLessThanMin)
{
    static auto const expectedOutput = fmt::format(
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
        LEDGERHASH,
        ACCOUNT,
        TOKENID,
        FLAG,
        ISSUER,
        TAXON,
        SERIAL,
        AccountNFTsHandler::LIMIT_MIN
    );

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = CreateAccountRootObject(ACCOUNT, 0, 1, 10, 2, TXNID, 3);
    auto const accountID = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    ON_CALL(*backend, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        AccountNFTsHandler::LIMIT_MIN - 1
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountNFTsHandlerTest, LimitMoreThanMax)
{
    static auto const expectedOutput = fmt::format(
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
        LEDGERHASH,
        ACCOUNT,
        TOKENID,
        FLAG,
        ISSUER,
        TAXON,
        SERIAL,
        AccountNFTsHandler::LIMIT_MAX
    );

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const accountObject = CreateAccountRootObject(ACCOUNT, 0, 1, 10, 2, TXNID, 3);
    auto const accountID = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(accountID).key, 30, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    auto const firstPage = ripple::keylet::nftpage_max(accountID).key;
    auto const pageObject =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    ON_CALL(*backend, doFetchLedgerObject(firstPage, 30, _))
        .WillByDefault(Return(pageObject.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        AccountNFTsHandler::LIMIT_MAX + 1
    ));
    auto const handler = AnyHandler{AccountNFTsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}
