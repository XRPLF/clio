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
#include "rpc/handlers/AccountObjects.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kISSUER = "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kTXN_ID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";
constexpr auto kTOKEN_ID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr auto kMAX_SEQ = 30;
constexpr auto kMIN_SEQ = 10;

}  // namespace

struct RPCAccountObjectsHandlerTest : HandlerBaseTest {
    RPCAccountObjectsHandlerTest()
    {
        backend_->setRange(kMIN_SEQ, kMAX_SEQ);
    }
};

struct AccountObjectsParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountObjectsParameterTest : public RPCAccountObjectsHandlerTest,
                                     public WithParamInterface<AccountObjectsParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountObjectsParamTestCaseBundle>{
        AccountObjectsParamTestCaseBundle{
            .testName = "MissingAccount",
            .testJson = R"({})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "AccountNotString",
            .testJson = R"({"account":1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "AccountInvalid",
            .testJson = R"({"account":"xxx"})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "TypeNotString",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "TypeInvalid",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":"wrong"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type'."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "TypeNotAccountOwned",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":"amendments"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'type'."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LedgerHashInvalid",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":"1"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LedgerHashNotString",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_index":"a"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LimitNotInt",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":"1"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LimitNagetive",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":-1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "LimitZero",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":0})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "MarkerNotString",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":9})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "markerNotString"
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "MarkerInvalid",
            .testJson = R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":"xxxx"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Malformed cursor."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "NFTMarkerInvalid",
            .testJson = fmt::format(
                R"({{"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":"wronghex256,{}"}})",
                std::numeric_limits<uint32_t>::max()
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Malformed cursor."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "DeletionBlockersOnlyInvalidString",
            .testJson = R"({"account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "deletion_blockers_only": "wrong"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            .testName = "DeletionBlockersOnlyInvalidNull",
            .testJson = R"({"account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "deletion_blockers_only": null})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountObjectsGroup1,
    AccountObjectsParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountObjectsParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaIntSequence)
{
    // return empty ledgerHeader
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ, _)).WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":30
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaStringSequence)
{
    // return empty ledgerHeader
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ, _)).WillOnce(Return(std::nullopt));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"30"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaHash)
{
    // return empty ledgerHeader
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, AccountNotExist)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);

    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DefaultParameterNoNFTFound)
{
    static constexpr auto kEXPECTED_OUT = R"({
                                            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                            "ledger_index":30,
                                            "validated":true,
                                            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "limit": 200,
                                            "account_objects":[
                                                {
                                                    "Balance":{
                                                        "currency":"USD",
                                                        "issuer":"rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
                                                        "value":"100"
                                                    },
                                                    "Flags":0,
                                                    "HighLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                        "value":"20"
                                                    },
                                                    "LedgerEntryType":"RippleState",
                                                    "LowLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                        "value":"10"
                                                    },
                                                    "PreviousTxnID":"E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879",
                                                    "PreviousTxnLgrSeq":123,
                                                    "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                                                }
                                            ]
                                        })";

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Limit)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static constexpr auto kLIMIT = 10;
    auto count = kLIMIT * 2;
    // put 20 items in owner dir, but only return 10
    auto const ownerDir = createOwnerDirLedgerObject(std::vector(count, ripple::uint256{kINDEX1}), kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 =
            createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        kLIMIT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), kLIMIT);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), fmt::format("{},{}", kINDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Marker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static constexpr auto kLIMIT = 20;
    static constexpr auto kPAGE = 2;
    auto count = kLIMIT;
    auto const ownerDir = createOwnerDirLedgerObject(std::vector(count, ripple::uint256{kINDEX1}), kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, kPAGE).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(hintIndex, 30, _))
        .Times(2)
        .WillRepeatedly(Return(ownerDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 =
            createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        kACCOUNT,
        kINDEX1,
        kPAGE
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), kLIMIT - 1);
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, MultipleDirNoNFT)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static constexpr auto kCOUNT = 10;
    static constexpr auto kNEXTPAGE = 1;
    auto cc = kCOUNT;
    auto ownerDir = createOwnerDirLedgerObject(std::vector(cc, ripple::uint256{kINDEX1}), kINDEX1);
    // set next page
    ownerDir.setFieldU64(ripple::sfIndexNext, kNEXTPAGE);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    auto const page1 = ripple::keylet::page(ownerDirKk, kNEXTPAGE).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(page1, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // 10 items per page, 2 pages
    cc = kCOUNT * 2;
    while (cc-- != 0) {
        auto const line1 =
            createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        2 * kCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), kCOUNT * 2);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), fmt::format("{},{}", kINDEX1, kNEXTPAGE));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilter)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 state and 1 offer
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
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
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type":"offer"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 1);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterAmmType)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 state and 1 amm
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    auto const ammObject = createAmmObject(kACCOUNT, "XRP", toBase58(ripple::xrpAccount()), "JPY", kACCOUNT2);
    bbs.push_back(ammObject.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "type": "amm"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        auto const& accountObjects = output.result->as_object().at("account_objects").as_array();
        ASSERT_EQ(accountObjects.size(), 1);
        EXPECT_EQ(accountObjects.front().at("LedgerEntryType").as_string(), "AMM");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterReturnEmpty)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
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
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type": "check"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilter)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);

    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const line = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    auto const channel = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
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

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 2);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterWithTypeFilter)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const line = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    auto const channel = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true,
            "type": "payment_channel"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 1);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterEmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const offer1 = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );
    auto const offer2 = createOfferLedgerObject(
        kACCOUNT,
        20,
        30,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(offer1.getSerializer().peekData());
    bbs.push_back(offer2.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterWithIncompatibleTypeYieldsEmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));
    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const offer1 = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );
    auto const offer2 = createOfferLedgerObject(
        kACCOUNT,
        20,
        30,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT2,
        toBase58(ripple::xrpAccount()),
        kINDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(offer1.getSerializer().peekData());
    bbs.push_back(offer2.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true,
            "type": "offer"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMixOtherObjects)
{
    static constexpr auto kEXPECTED_OUT = R"({
                                            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                            "ledger_index":30,
                                            "validated":true,
                                            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "limit": 200,
                                            "account_objects":[
                                                {
                                                    "Flags":0,
                                                    "LedgerEntryType":"NFTokenPage",
                                                    "NFTokens":[
                                                        {
                                                            "NFToken":{
                                                                "NFTokenID":"000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA",
                                                                "URI":"7777772E6F6B2E636F6D"
                                                            }
                                                        }
                                                    ],
                                                    "PreviousPageMin":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9659B25014D08E1BC983515BC",
                                                    "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                                                    "PreviousTxnLgrSeq":0,
                                                    "index":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9FFFFFFFFFFFFFFFFFFFFFFFF"
                                                },
                                                {
                                                    "Flags":0,
                                                    "LedgerEntryType":"NFTokenPage",
                                                    "NFTokens":[
                                                        {
                                                            "NFToken":{
                                                                "NFTokenID":"000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA",
                                                                "URI":"7777772E6F6B2E636F6D"
                                                            }
                                                        }
                                                    ],
                                                    "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                                                    "PreviousTxnLgrSeq":0,
                                                    "index":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9659B25014D08E1BC983515BC"
                                                },
                                                {
                                                    "Balance":{
                                                        "currency":"USD",
                                                        "issuer":"rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
                                                        "value":"100"
                                                    },
                                                    "Flags":0,
                                                    "HighLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                        "value":"20"
                                                    },
                                                    "LedgerEntryType":"RippleState",
                                                    "LowLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                        "value":"10"
                                                    },
                                                    "PreviousTxnID":"E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879",
                                                    "PreviousTxnLgrSeq":123,
                                                    "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                                                }
                                            ]
                                        })";

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft page 1
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    auto const nftPage2KK = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{kINDEX1}).key;
    auto const nftpage1 =
        createNftTokenPage(std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, nftPage2KK);
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(nftpage1.getSerializer().peekData()));

    // nft page 2 , end
    auto const nftpage2 = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftPage2KK, 30, _))
        .WillOnce(Return(nftpage2.getSerializer().peekData()));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTReachLimitReturnMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto current = ripple::keylet::nftpage_max(account).key;
    std::string first{kINDEX1};
    std::ranges::sort(first);
    for (auto i = 0; i < 10; i++) {
        std::ranges::next_permutation(first);
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage = createNftTokenPage(
            std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, previous
        );
        EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _))
            .WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        10
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("account_objects").as_array().size(), 10);
        EXPECT_EQ(
            output.result.value().as_object().at("marker").as_string(),
            fmt::format("{},{}", ripple::strHex(current), std::numeric_limits<uint32_t>::max())
        );
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTReachLimitNoMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto current = ripple::keylet::nftpage_max(account).key;
    std::string first{kINDEX1};
    std::ranges::sort(first);
    for (auto i = 0; i < 10; i++) {
        std::ranges::next_permutation(first);
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage = createNftTokenPage(
            std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, previous
        );
        EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _))
            .WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        kACCOUNT,
        11
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("account_objects").as_array().size(), 11);
        //"0000000000000000000000000000000000000000000000000000000000000000,4294967295"
        EXPECT_EQ(
            output.result.value().as_object().at("marker").as_string(),
            fmt::format("{},{}", ripple::strHex(ripple::uint256(beast::zero)), std::numeric_limits<uint32_t>::max())
        );
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    std::string first{kINDEX1};
    auto current = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
    auto const marker = current;
    std::ranges::sort(first);
    for (auto i = 0; i < 10; i++) {
        std::ranges::next_permutation(first);
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage = createNftTokenPage(
            std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, previous
        );
        EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _))
            .WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1
    );
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    auto const channel = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
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

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        kACCOUNT,
        ripple::strHex(marker),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("account_objects").as_array().size(), 11 + 3);
        EXPECT_FALSE(output.result.value().as_object().contains("marker"));
    });
}

// when limit reached, happen to be the end of NFT page list
TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNoMoreNFT)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1
    );
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    auto const channel = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
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

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        kACCOUNT,
        ripple::strHex(ripple::uint256{beast::zero}),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("account_objects").as_array().size(), 3);
        EXPECT_FALSE(output.result.value().as_object().contains("marker"));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNotInRange)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "marker" : "{},{}"
        }})",
        kACCOUNT,
        kINDEX1,
        std::numeric_limits<std::uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNotExist)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    // return null for this marker
    auto const accountNftMax = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountNftMax, kMAX_SEQ, _)).WillOnce(Return(std::nullopt));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "marker" : "{},{}"
        }})",
        kACCOUNT,
        ripple::strHex(accountNftMax),
        std::numeric_limits<std::uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTLimitAdjust)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    std::string first{kINDEX1};
    auto current = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
    auto const marker = current;
    std::ranges::sort(first);
    for (auto i = 0; i < 10; i++) {
        std::ranges::next_permutation(first);
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage = createNftTokenPage(
            std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, previous
        );
        EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _))
            .WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    auto const channel = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
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

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}",
            "limit": 12 
        }})",
        kACCOUNT,
        ripple::strHex(marker),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("account_objects").as_array().size(), 12);
        // marker not in NFT "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC,0"
        EXPECT_EQ(output.result.value().as_object().at("marker").as_string(), fmt::format("{},{}", kINDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, FilterNFT)
{
    static constexpr auto kEXPECTED_OUT = R"({
                                            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                            "ledger_index":30,
                                            "validated":true,
                                            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "limit": 200,
                                            "account_objects":[
                                                {
                                                    "Flags":0,
                                                    "LedgerEntryType":"NFTokenPage",
                                                    "NFTokens":[
                                                        {
                                                            "NFToken":{
                                                                "NFTokenID":"000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA",
                                                                "URI":"7777772E6F6B2E636F6D"
                                                            }
                                                        }
                                                    ],
                                                    "PreviousPageMin":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9659B25014D08E1BC983515BC",
                                                    "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                                                    "PreviousTxnLgrSeq":0,
                                                    "index":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9FFFFFFFFFFFFFFFFFFFFFFFF"
                                                },
                                                {
                                                    "Flags":0,
                                                    "LedgerEntryType":"NFTokenPage",
                                                    "NFTokens":[
                                                        {
                                                            "NFToken":{
                                                                "NFTokenID":"000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA",
                                                                "URI":"7777772E6F6B2E636F6D"
                                                            }
                                                        }
                                                    ],
                                                    "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                                                    "PreviousTxnLgrSeq":0,
                                                    "index":"4B4E9C06F24296074F7BC48F92A97916C6DC5EA9659B25014D08E1BC983515BC"
                                                }
                                            ]
                                        })";

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft page 1
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    auto const nftPage2KK = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{kINDEX1}).key;
    auto const nftpage1 =
        createNftTokenPage(std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, nftPage2KK);
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(nftpage1.getSerializer().peekData()));

    // nft page 2 , end
    auto const nftpage2 = createNftTokenPage(
        std::vector{std::make_pair<std::string, std::string>(kTOKEN_ID, "www.ok.com")}, std::nullopt
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftPage2KK, 30, _))
        .WillOnce(Return(nftpage2.getSerializer().peekData()));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type": "nft_page"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTZeroMarkerNotAffectOtherMarker)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static constexpr auto kLIMIT = 10;
    auto count = kLIMIT * 2;
    // put 20 items in owner dir, but only return 10
    auto const ownerDir = createOwnerDirLedgerObject(std::vector(count, ripple::uint256{kINDEX1}), kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 =
            createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{},
            "marker": "{},{}"
        }})",
        kACCOUNT,
        kLIMIT,
        ripple::strHex(ripple::uint256{beast::zero}),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("account_objects").as_array().size(), kLIMIT);
        EXPECT_EQ(output.result->as_object().at("marker").as_string(), fmt::format("{},{}", kINDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LimitLessThanMin)
{
    static auto const kEXPECTED_OUT = fmt::format(
        R"({{
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "limit": {},
            "account_objects":[
                {{
                    "Balance":{{
                        "currency":"USD",
                        "issuer":"rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
                        "value":"100"
                    }},
                    "Flags":0,
                    "HighLimit":{{
                        "currency":"USD",
                        "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "value":"20"
                    }},
                    "LedgerEntryType":"RippleState",
                    "LowLimit":{{
                        "currency":"USD",
                        "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "value":"10"
                    }},
                    "PreviousTxnID":"E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879",
                    "PreviousTxnLgrSeq":123,
                    "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                }}
            ]
        }})",
        AccountObjectsHandler::kLIMIT_MIN
    );

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit": {}
        }})",
        kACCOUNT,
        AccountObjectsHandler::kLIMIT_MIN - 1
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LimitMoreThanMax)
{
    static auto const kEXPECTED_OUT = fmt::format(
        R"({{
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "limit": {},
            "account_objects":[
                {{
                    "Balance":{{
                        "currency":"USD",
                        "issuer":"rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
                        "value":"100"
                    }},
                    "Flags":0,
                    "HighLimit":{{
                        "currency":"USD",
                        "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "value":"20"
                    }},
                    "LedgerEntryType":"RippleState",
                    "LowLimit":{{
                        "currency":"USD",
                        "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "value":"10"
                    }},
                    "PreviousTxnID":"E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879",
                    "PreviousTxnLgrSeq":123,
                    "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                }}
            ]
        }})",
        AccountObjectsHandler::kLIMIT_MAX
    );

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit": {}
        }})",
        kACCOUNT,
        AccountObjectsHandler::kLIMIT_MAX + 1
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterMPTIssuanceType)
{
    auto const ledgerinfo = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 mpt issuance
    auto const issuanceObject = createMptIssuanceObject(kACCOUNT, 2, "metadata");
    bbs.push_back(issuanceObject.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "type": "mpt_issuance"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        auto const& accountObjects = output.result->as_object().at("account_objects").as_array();
        ASSERT_EQ(accountObjects.size(), 1);
        EXPECT_EQ(accountObjects.front().at("LedgerEntryType").as_string(), "MPTokenIssuance");

        // make sure mptID is synethetically parsed if object is mptIssuance
        EXPECT_EQ(
            accountObjects.front().at("mpt_issuance_id").as_string(),
            ripple::to_string(ripple::makeMptID(2, getAccountIdWithString(kACCOUNT)))
        );
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterMPTokenType)
{
    auto const ledgerinfo = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, kMAX_SEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 mpt issuance
    auto const mptokenObject = createMpTokenObject(kACCOUNT, ripple::makeMptID(2, getAccountIdWithString(kACCOUNT)));
    bbs.push_back(mptokenObject.getSerializer().peekData());

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "type": "mptoken"
        }})",
        kACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        auto const& accountObjects = output.result->as_object().at("account_objects").as_array();
        ASSERT_EQ(accountObjects.size(), 1);
        EXPECT_EQ(accountObjects.front().at("LedgerEntryType").as_string(), "MPToken");
    });
}
