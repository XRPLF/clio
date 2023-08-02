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

#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/AccountOffers.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

using namespace RPC;
namespace json = boost::json;
using namespace testing;

class RPCAccountOffersHandlerTest : public HandlerBaseTest
{
};

struct AccountOfferParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountOfferParameterTest : public RPCAccountOffersHandlerTest,
                                   public WithParamInterface<AccountOfferParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<AccountOfferParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountOfferParamTestCaseBundle>{
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
            "Malformed cursor.",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountOffersGroup1,
    AccountOfferParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AccountOfferParameterTest::NameGenerator{});

TEST_P(AccountOfferParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaStringIndex)
{
    auto constexpr seq = 12;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"{}"
        }})",
        ACCOUNT,
        seq));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LedgerNotFoundViaIntIndex)
{
    auto constexpr seq = 12;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":{}
        }})",
        ACCOUNT,
        seq));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, AccountNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountOffersHandlerTest, DefaultParams)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true,
            "account":"{}",
            "offers":
            [
                {{
                    "seq":0,
                    "flags":0,
                    "quality":"0.000000024999999374023",
                    "taker_pays":"20",
                    "taker_gets":
                    {{
                        "currency":"USD",
                        "issuer":"{}",
                        "value":"10"
                    }},
                    "expiration":123
                }}
            ]
        }})",
        LEDGERHASH,
        ACCOUNT,
        ACCOUNT2);
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1);
    offer.setFieldU32(ripple::sfExpiration, 123);
    bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Limit)
{
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector{20, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++)
    {
        auto const offer = CreateOfferLedgerObject(
            ACCOUNT,
            10,
            20,
            ripple::to_string(ripple::to_currency("USD")),
            ripple::to_string(ripple::xrpCurrency()),
            ACCOUNT2,
            toBase58(ripple::xrpAccount()),
            INDEX1);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":10
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), 10);
        EXPECT_EQ(output->at("marker").as_string(), fmt::format("{},0", INDEX1));
    });
}

TEST_F(RPCAccountOffersHandlerTest, Marker)
{
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector{20, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(hintIndex, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    for (auto i = 0; i < 20; i++)
    {
        auto const offer = CreateOfferLedgerObject(
            ACCOUNT,
            10,
            20,
            ripple::to_string(ripple::to_currency("USD")),
            ripple::to_string(ripple::xrpCurrency()),
            ACCOUNT2,
            toBase58(ripple::xrpAccount()),
            INDEX1);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        INDEX1,
        startPage));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), 19);
        EXPECT_FALSE(output->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountOffersHandlerTest, MarkerNotExists)
{
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const startPage = 2;
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, startPage).key;

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(hintIndex, ledgerSeq, _)).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        INDEX1,
        startPage));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid marker.");
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitLessThanMin)
{
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        CreateOwnerDirLedgerObject(std::vector{AccountOffersHandler::LIMIT_MIN + 1, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1);
    offer.setFieldU32(ripple::sfExpiration, 123);

    for (auto i = 0; i < AccountOffersHandler::LIMIT_MIN + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        AccountOffersHandler::LIMIT_MIN - 1));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), AccountOffersHandler::LIMIT_MIN);
    });
}

TEST_F(RPCAccountOffersHandlerTest, LimitMoreThanMax)
{
    auto constexpr ledgerSeq = 30;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);         // min
    mockBackendPtr->updateRange(ledgerSeq);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        CreateOwnerDirLedgerObject(std::vector{AccountOffersHandler::LIMIT_MAX + 1, ripple::uint256{INDEX1}}, INDEX1);

    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1);
    offer.setFieldU32(ripple::sfExpiration, 123);
    for (auto i = 0; i < AccountOffersHandler::LIMIT_MAX + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        AccountOffersHandler::LIMIT_MAX + 1));
    auto const handler = AnyHandler{AccountOffersHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), AccountOffersHandler::LIMIT_MAX);
    });
}
