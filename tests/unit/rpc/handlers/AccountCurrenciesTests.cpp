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
#include "rpc/handlers/AccountCurrencies.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTXN_ID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

}  // namespace

struct RPCAccountCurrenciesHandlerTest : HandlerBaseTest {
    RPCAccountCurrenciesHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCAccountCurrenciesHandlerTest, AccountNotExist)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
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
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaStringSequence)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(12, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"{}"
        }})",
        kACCOUNT,
        kSEQ
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaHash)
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
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, DefaultParameter)
{
    static constexpr auto kOUTPUT = R"({
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "receive_currencies":[
            "EUR",
            "JPY"
        ],
        "send_currencies":[
            "EUR",
            "USD"
        ]
    })";

    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(30, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}}, kINDEX1
    );
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // Account can receive USD 10 from Account2 and send USD 20 to Account2, now
    // the balance is 100, Account can only send USD to Account2
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    // Account2 can receive JPY 10 from Account and send JPY 20 to Account, now
    // the balance is 100, Account2 can only send JPY to Account
    auto const line2 = createRippleStateLedgerObject("JPY", kISSUER, 100, kACCOUNT2, 10, kACCOUNT, 20, kTXN_ID, 123, 0);
    // Account can receive EUR 10 from Account and send EUR 20 to Account2, now
    // the balance is 8, Account can receive/send EUR to/from Account2
    auto const line3 = createRippleStateLedgerObject("EUR", kISSUER, 8, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    bbs.push_back(line3.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kOUTPUT));
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderHash)
{
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderSeq)
{
    auto const ledgerSeq = 29;
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, ledgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(ledgerSeq, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, ledgerSeq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":{}
        }})",
        kACCOUNT,
        ledgerSeq
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("ledger_index").as_uint64(), ledgerSeq);
    });
}

TEST(RPCAccountCurrenciesHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"account", "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59"},
        {"ledger_hash", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"ledger_index", 30},
        {"account_index", 1},
        {"strict", true}
    };
    auto const spec = AccountCurrenciesHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated));
    for (auto const& field : {"account_index", "strict"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)), std::string::npos
        );
    }
}
