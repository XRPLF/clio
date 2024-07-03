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

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto ISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto TXNID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

class RPCAccountCurrenciesHandlerTest : public HandlerBaseTest {};

TEST_F(RPCAccountCurrenciesHandlerTest, AccountNotExist)
{
    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
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
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaIntSequence)
{
    backend->setRange(10, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaStringSequence)
{
    auto constexpr seq = 12;

    backend->setRange(10, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(12, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"{}"
        }})",
        ACCOUNT,
        seq
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaHash)
{
    backend->setRange(10, 30);
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
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, DefaultParameter)
{
    auto constexpr static OUTPUT = R"({
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

    backend->setRange(10, 30);
    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence(30, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir =
        CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    // ACCOUNT can receive USD 10 from ACCOUNT2 and send USD 20 to ACCOUNT2, now
    // the balance is 100, ACCOUNT can only send USD to ACCOUNT2
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    // ACCOUNT2 can receive JPY 10 from ACCOUNT and send JPY 20 to ACCOUNT, now
    // the balance is 100, ACCOUNT2 can only send JPY to ACCOUNT
    auto const line2 = CreateRippleStateLedgerObject("JPY", ISSUER, 100, ACCOUNT2, 10, ACCOUNT, 20, TXNID, 123, 0);
    // ACCOUNT can receive EUR 10 from ACCOUNT and send EUR 20 to ACCOUNT2, now
    // the balance is 8, ACCOUNT can receive/send EUR to/from ACCOUNT2
    auto const line3 = CreateRippleStateLedgerObject("EUR", ISSUER, 8, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    bbs.push_back(line3.getSerializer().peekData());

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);
    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUTPUT));
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderHash)
{
    backend->setRange(10, 30);
    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);
    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderSeq)
{
    backend->setRange(10, 30);
    auto const ledgerSeq = 29;
    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, ledgerSeq);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence(ledgerSeq, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, ledgerSeq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = CreateRippleStateLedgerObject("USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);
    auto static const input = json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":{}
        }})",
        ACCOUNT,
        ledgerSeq
    ));
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
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
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::warnRPC_DEPRECATED));
    for (auto const& field : {"account_index", "strict"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)), std::string::npos
        );
    }
}
