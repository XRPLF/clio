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

#include "data/Types.h"
#include "rpc/Errors.h"
#include "rpc/common/AnyHandler.h"
#include "rpc/common/Types.h"
#include "rpc/handlers/AccountObjects.h"
#include "util/Fixtures.h"
#include "util/TestObject.h"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/beast/utility/Zero.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/UintTypes.h>

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

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ISSUER = "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto TXNID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";
constexpr static auto TOKENID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr static auto MAXSEQ = 30;
constexpr static auto MINSEQ = 10;

class RPCAccountObjectsHandlerTest : public HandlerBaseTest {};

struct AccountObjectsParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountObjectsParameterTest : public RPCAccountObjectsHandlerTest,
                                     public WithParamInterface<AccountObjectsParamTestCaseBundle> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<AccountObjectsParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountObjectsParamTestCaseBundle>{
        AccountObjectsParamTestCaseBundle{
            "MissingAccount", R"({})", "invalidParams", "Required field 'account' missing"
        },
        AccountObjectsParamTestCaseBundle{"AccountNotString", R"({"account":1})", "invalidParams", "accountNotString"},
        AccountObjectsParamTestCaseBundle{"AccountInvalid", R"({"account":"xxx"})", "actMalformed", "accountMalformed"},
        AccountObjectsParamTestCaseBundle{
            "TypeNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":1})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            "TypeInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":"wrong"})",
            "invalidParams",
            "Invalid field 'type'."
        },
        AccountObjectsParamTestCaseBundle{
            "LedgerHashInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":"1"})",
            "invalidParams",
            "ledger_hashMalformed"
        },
        AccountObjectsParamTestCaseBundle{
            "LedgerHashNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":1})",
            "invalidParams",
            "ledger_hashNotString"
        },
        AccountObjectsParamTestCaseBundle{
            "LedgerIndexInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_index":"a"})",
            "invalidParams",
            "ledgerIndexMalformed"
        },
        AccountObjectsParamTestCaseBundle{
            "LimitNotInt",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":"1"})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            "LimitNagetive",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":-1})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            "LimitZero",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":0})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            "MarkerNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":9})",
            "invalidParams",
            "markerNotString"
        },
        AccountObjectsParamTestCaseBundle{
            "MarkerInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":"xxxx"})",
            "invalidParams",
            "Malformed cursor."
        },
        AccountObjectsParamTestCaseBundle{
            "NFTMarkerInvalid",
            fmt::format(
                R"({{"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":"wronghex256,{}"}})",
                std::numeric_limits<uint32_t>::max()
            ),
            "invalidParams",
            "Malformed cursor."
        },
        AccountObjectsParamTestCaseBundle{
            "DeletionBlockersOnlyInvalidString",
            R"({"account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "deletion_blockers_only": "wrong"})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountObjectsParamTestCaseBundle{
            "DeletionBlockersOnlyInvalidNull",
            R"({"account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "deletion_blockers_only": null})",
            "invalidParams",
            "Invalid parameters."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountObjectsGroup1,
    AccountObjectsParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AccountObjectsParameterTest::NameGenerator{}
);

TEST_P(AccountObjectsParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountObjectsHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaIntSequence)
{
    backend->setRange(MINSEQ, MAXSEQ);
    // return empty ledgerinfo
    EXPECT_CALL(*backend, fetchLedgerBySequence(MAXSEQ, _)).WillOnce(Return(std::optional<ripple::LedgerInfo>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":30
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaStringSequence)
{
    backend->setRange(MINSEQ, MAXSEQ);
    // return empty ledgerinfo
    EXPECT_CALL(*backend, fetchLedgerBySequence(MAXSEQ, _)).WillOnce(Return(std::nullopt));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"30"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaHash)
{
    backend->setRange(MINSEQ, MAXSEQ);
    // return empty ledgerinfo
    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillOnce(Return(std::optional<ripple::LedgerInfo>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, AccountNotExist)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);

    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DefaultParameterNoNFTFound)
{
    static auto constexpr expectedOut = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Limit)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr limit = 10;
    auto count = limit * 2;
    // put 20 items in owner dir, but only return 10
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector(count, ripple::uint256{INDEX1}), INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        limit
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), limit);
        EXPECT_EQ(output->as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Marker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr limit = 20;
    static auto constexpr page = 2;
    auto count = limit;
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector(count, ripple::uint256{INDEX1}), INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, page).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(hintIndex, 30, _))
        .Times(2)
        .WillRepeatedly(Return(ownerDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        INDEX1,
        page
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), limit - 1);
        EXPECT_FALSE(output->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, MultipleDirNoNFT)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr count = 10;
    static auto constexpr nextpage = 1;
    auto cc = count;
    auto ownerDir = CreateOwnerDirLedgerObject(std::vector(cc, ripple::uint256{INDEX1}), INDEX1);
    // set next page
    ownerDir.setFieldU64(ripple::sfIndexNext, nextpage);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    auto const page1 = ripple::keylet::page(ownerDirKk, nextpage).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject(page1, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // 10 items per page, 2 pages
    cc = count * 2;
    while (cc-- != 0) {
        auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        2 * count
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), count * 2);
        EXPECT_EQ(output->as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, nextpage));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilter)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 state and 1 offer
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type":"offer"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 1);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterAmmType)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    // put 1 state and 1 amm
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    auto const ammObject = CreateAMMObject(ACCOUNT, "XRP", toBase58(ripple::xrpAccount()), "JPY", ACCOUNT2);
    bbs.push_back(ammObject.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "type": "amm"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& accountObjects = output->as_object().at("account_objects").as_array();
        ASSERT_EQ(accountObjects.size(), 1);
        EXPECT_EQ(accountObjects.front().at("LedgerEntryType").as_string(), "AMM");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterReturnEmpty)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type": "check"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilter)
{
    backend->setRange(MINSEQ, MAXSEQ);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);

    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const line = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 2);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterWithTypeFilter)
{
    backend->setRange(MINSEQ, MAXSEQ);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const line = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true,
            "type": "payment_channel"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 1);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterEmptyResult)
{
    backend->setRange(MINSEQ, MAXSEQ);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const offer1 = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );
    auto const offer2 = CreateOfferLedgerObject(
        ACCOUNT,
        20,
        30,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(offer1.getSerializer().peekData());
    bbs.push_back(offer2.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DeletionBlockersOnlyFilterWithIncompatibleTypeYieldsEmptyResult)
{
    backend->setRange(MINSEQ, MAXSEQ);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));
    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    auto const offer1 = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );
    auto const offer2 = CreateOfferLedgerObject(
        ACCOUNT,
        20,
        30,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(offer1.getSerializer().peekData());
    bbs.push_back(offer2.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "deletion_blockers_only": true,
            "type": "offer"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 0);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMixOtherObjects)
{
    static auto constexpr expectedOut = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft page 1
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    auto const nftPage2KK = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{INDEX1}).key;
    auto const nftpage1 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, nftPage2KK);
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(nftpage1.getSerializer().peekData()));

    // nft page 2 , end
    auto const nftpage2 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    EXPECT_CALL(*backend, doFetchLedgerObject(nftPage2KK, 30, _)).WillOnce(Return(nftpage2.getSerializer().peekData()));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTReachLimitReturnMarker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto current = ripple::keylet::nftpage_max(account).key;
    std::string first{INDEX1};
    sort(first.begin(), first.end());
    for (auto i = 0; i < 10; i++) {
        std::next_permutation(first.begin(), first.end());
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage =
            CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, previous);
        EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        10
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().as_object().at("account_objects").as_array().size(), 10);
        EXPECT_EQ(
            output.value().as_object().at("marker").as_string(),
            fmt::format("{},{}", ripple::strHex(current), std::numeric_limits<uint32_t>::max())
        );
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTReachLimitNoMarker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto current = ripple::keylet::nftpage_max(account).key;
    std::string first{INDEX1};
    sort(first.begin(), first.end());
    for (auto i = 0; i < 10; i++) {
        std::next_permutation(first.begin(), first.end());
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage =
            CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, previous);
        EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        11
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().as_object().at("account_objects").as_array().size(), 11);
        //"0000000000000000000000000000000000000000000000000000000000000000,4294967295"
        EXPECT_EQ(
            output.value().as_object().at("marker").as_string(),
            fmt::format("{},{}", ripple::strHex(ripple::uint256(beast::zero)), std::numeric_limits<uint32_t>::max())
        );
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    std::string first{INDEX1};
    auto current = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
    auto const marker = current;
    sort(first.begin(), first.end());
    for (auto i = 0; i < 10; i++) {
        std::next_permutation(first.begin(), first.end());
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage =
            CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, previous);
        EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto const ownerDir =
        CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        ripple::strHex(marker),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().as_object().at("account_objects").as_array().size(), 11 + 3);
        EXPECT_FALSE(output.value().as_object().contains("marker"));
    });
}

// when limit reached, happen to be the end of NFT page list
TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNoMoreNFT)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        ripple::strHex(ripple::uint256{beast::zero}),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().as_object().at("account_objects").as_array().size(), 3);
        EXPECT_FALSE(output.value().as_object().contains("marker"));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNotInRange)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "marker" : "{},{}"
        }})",
        ACCOUNT,
        INDEX1,
        std::numeric_limits<std::uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTMarkerNotExist)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    // return null for this marker
    auto const accountNftMax = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountNftMax, MAXSEQ, _)).WillOnce(Return(std::nullopt));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "marker" : "{},{}"
        }})",
        ACCOUNT,
        ripple::strHex(accountNftMax),
        std::numeric_limits<std::uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTLimitAdjust)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    std::string first{INDEX1};
    auto current = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
    auto const marker = current;
    sort(first.begin(), first.end());
    for (auto i = 0; i < 10; i++) {
        std::next_permutation(first.begin(), first.end());
        auto previous =
            ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{first.c_str()}).key;
        auto const nftpage =
            CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, previous);
        EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage.getSerializer().peekData()));
        current = previous;
    }
    auto const nftpage11 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    EXPECT_CALL(*backend, doFetchLedgerObject(current, 30, _)).WillOnce(Return(nftpage11.getSerializer().peekData()));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const line = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1
    );

    std::vector<Blob> bbs;
    bbs.push_back(line.getSerializer().peekData());
    bbs.push_back(channel.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}",
            "limit": 12 
        }})",
        ACCOUNT,
        ripple::strHex(marker),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().as_object().at("account_objects").as_array().size(), 12);
        // marker not in NFT "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC,0"
        EXPECT_EQ(output.value().as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, FilterNFT)
{
    static auto constexpr expectedOut = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft page 1
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    auto const nftPage2KK = ripple::keylet::nftpage(ripple::keylet::nftpage_min(account), ripple::uint256{INDEX1}).key;
    auto const nftpage1 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, nftPage2KK);
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(nftpage1.getSerializer().peekData()));

    // nft page 2 , end
    auto const nftpage2 =
        CreateNFTTokenPage(std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt);
    EXPECT_CALL(*backend, doFetchLedgerObject(nftPage2KK, 30, _)).WillOnce(Return(nftpage2.getSerializer().peekData()));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type": "nft_page"
        }})",
        ACCOUNT
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, NFTZeroMarkerNotAffectOtherMarker)
{
    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr limit = 10;
    auto count = limit * 2;
    // put 20 items in owner dir, but only return 10
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector(count, ripple::uint256{INDEX1}), INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    while (count-- != 0) {
        auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{},
            "marker": "{},{}"
        }})",
        ACCOUNT,
        limit,
        ripple::strHex(ripple::uint256{beast::zero}),
        std::numeric_limits<uint32_t>::max()
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), limit);
        EXPECT_EQ(output->as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LimitLessThanMin)
{
    static auto const expectedOut = fmt::format(
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
        AccountObjectsHandler::LIMIT_MIN
    );

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit": {}
        }})",
        ACCOUNT,
        AccountObjectsHandler::LIMIT_MIN - 1
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LimitMoreThanMax)
{
    static auto const expectedOut = fmt::format(
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
        AccountObjectsHandler::LIMIT_MAX
    );

    backend->setRange(MINSEQ, MAXSEQ);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _)).WillOnce(Return(ownerDir.getSerializer().peekData()));

    // nft null
    auto const nftMaxKK = ripple::keylet::nftpage_max(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(nftMaxKK, 30, _)).WillOnce(Return(std::nullopt));

    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    EXPECT_CALL(*backend, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit": {}
        }})",
        ACCOUNT,
        AccountObjectsHandler::LIMIT_MAX + 1
    ));

    auto const handler = AnyHandler{AccountObjectsHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}
