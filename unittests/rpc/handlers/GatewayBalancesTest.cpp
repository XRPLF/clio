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
#include <rpc/handlers/GatewayBalances.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto ACCOUNT3 = "raHGBERMka3KZsfpTQUAtumxmvpqhFLyrk";
constexpr static auto ISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto TXNID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

class RPCGatewayBalancesHandlerTest : public HandlerBaseTest
{
};

struct ParameterTestBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct ParameterTest : public RPCGatewayBalancesHandlerTest, public WithParamInterface<ParameterTestBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<ParameterTestBundle>(info.param);
            return bundle.testName;
        }
    };
};

TEST_P(ParameterTest, CheckError)
{
    auto bundle = GetParam();
    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(json::parse(bundle.testJson), Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), bundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), bundle.expectedErrorMessage);
    });
}

auto
generateParameterTestBundles()
{
    return std::vector<ParameterTestBundle>{
        ParameterTestBundle{
            "AccountNotString",
            R"({
                "account": 1213
            })",
            "invalidParams",
            "accountNotString"},
        ParameterTestBundle{
            "AccountMissing",
            R"({
            })",
            "invalidParams",
            "Required field 'account' missing"},
        ParameterTestBundle{
            "AccountInvalid",
            R"({
                "account": "1213"
            })",
            "actMalformed",
            "accountMalformed"},
        ParameterTestBundle{
            "LedgerIndexInvalid",
            fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": "meh"
                }})",
                ACCOUNT),
            "invalidParams",
            "ledgerIndexMalformed"},
        ParameterTestBundle{
            "LedgerHashInvalid",
            fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": "meh"
                }})",
                ACCOUNT),
            "invalidParams",
            "ledger_hashMalformed"},
        ParameterTestBundle{
            "LedgerHashNotString",
            fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": 12
                }})",
                ACCOUNT),
            "invalidParams",
            "ledger_hashNotString"},
        ParameterTestBundle{
            "WalletsNotStringOrArray",
            fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": 12
                }})",
                ACCOUNT),
            "invalidParams",
            "hotwalletNotStringOrArray"},
        ParameterTestBundle{
            "WalletsNotStringAccount",
            fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": [12]
                }})",
                ACCOUNT),
            "invalidParams",
            "hotwalletMalformed"},
        ParameterTestBundle{
            "WalletsInvalidAccount",
            fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": ["12"]
                }})",
                ACCOUNT),
            "invalidParams",
            "hotwalletMalformed"},
        ParameterTestBundle{
            "WalletInvalidAccount",
            fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": "12"
                }})",
                ACCOUNT),
            "invalidParams",
            "hotwalletMalformed"},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    ParameterTest,
    testing::ValuesIn(generateParameterTestBundles()),
    ParameterTest::NameGenerator());

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaStringIndex)
{
    auto const seq = 123;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(300);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": "{}"
                }})",
                ACCOUNT,
                seq)),
            Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaIntIndex)
{
    auto const seq = 123;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(300);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": {}
                }})",
                ACCOUNT,
                seq)),
            Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(300);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": "{}"
                }})",
                ACCOUNT,
                LEDGERHASH)),
            Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, AccountNotFound)
{
    auto const seq = 300;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerinfo));

    // return empty account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}"
                }})",
                ACCOUNT)),
            Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, InvalidHotWallet)
{
    auto const seq = 300;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerinfo));

    // return valid account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return valid owner dir
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // create a valid line, balance is 0
    auto const line1 = CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 0, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123);
    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": "{}"
                }})",
                ACCOUNT,
                ACCOUNT2)),
            Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidHotWallet");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid hot wallet.");
    });
}

struct NormalTestBundle
{
    std::string testName;
    ripple::STObject mockedDir;
    std::vector<ripple::STObject> mockedObjects;
    std::string expectedJson;
    std::string hotwallet;
};

struct NormalPathTest : public RPCGatewayBalancesHandlerTest, public WithParamInterface<NormalTestBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<NormalTestBundle>(info.param);
            return bundle.testName;
        }
    };
};

TEST_P(NormalPathTest, CheckOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerinfo));

    // return valid account
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return valid owner dir
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(bundle.mockedDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    std::transform(
        bundle.mockedObjects.begin(), bundle.mockedObjects.end(), std::back_inserter(bbs), [](auto const& obj) {
            return obj.getSerializer().peekData();
        });
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    {}
                }})",
                ACCOUNT,
                bundle.hotwallet)),
            Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value(), json::parse(bundle.expectedJson));
    });
}

auto
generateNormalPathTestBundles()
{
    auto frozenState =
        CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, -50, ACCOUNT, 10, ACCOUNT3, 20, TXNID, 123);
    frozenState.setFieldU32(ripple::sfFlags, ripple::lsfLowFreeze);

    auto overflowState =
        CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, 50, ACCOUNT, 10, ACCOUNT3, 20, TXNID, 123);
    int64_t min64 = -9922966390934554;
    overflowState.setFieldAmount(ripple::sfBalance, ripple::STAmount(GetIssue("JPY", ISSUER), min64, 80));
    return std::vector<NormalTestBundle>{
        NormalTestBundle{
            "AllBranches",
            CreateOwnerDirLedgerObject(
                {ripple::uint256{INDEX2},
                 ripple::uint256{INDEX2},
                 ripple::uint256{INDEX2},
                 ripple::uint256{INDEX2},
                 ripple::uint256{INDEX2},
                 ripple::uint256{INDEX2}},
                INDEX1),
            std::vector{
                // hotwallet
                CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, -10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123),
                // hotwallet
                CreateRippleStateLedgerObject(ACCOUNT, "CNY", ISSUER, -20, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123),
                // positive balance -> asset
                CreateRippleStateLedgerObject(ACCOUNT, "EUR", ISSUER, 30, ACCOUNT, 100, ACCOUNT3, 200, TXNID, 123),
                // positive balance -> asset
                CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, 40, ACCOUNT, 100, ACCOUNT3, 200, TXNID, 123),
                // obligation
                CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, -50, ACCOUNT, 10, ACCOUNT3, 20, TXNID, 123),
                frozenState

            },
            fmt::format(
                R"({{
                    "obligations":{{
                        "JPY":"50"
                    }},
                    "balances":{{
                        "{}":[
                            {{
                                "currency":"USD",
                                "value":"10"
                            }},
                            {{
                                "currency":"CNY",
                                "value":"20"
                            }}
                        ]
                    }},
                    "frozen_balances":{{
                        "{}":[
                            {{
                                "currency":"JPY",
                                "value":"50"
                            }}
                        ]
                    }},
                    "assets":{{
                        "{}":[
                            {{
                                "currency":"EUR",
                                "value":"30"
                            }},
                            {{
                                "currency":"JPY",
                                "value":"40"
                            }}
                        ]
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                ACCOUNT2,
                ACCOUNT3,
                ACCOUNT3,
                ACCOUNT),
            fmt::format(R"("hotwallet": "{}")", ACCOUNT2)},
        NormalTestBundle{
            "NoHotwallet",
            CreateOwnerDirLedgerObject({ripple::uint256{INDEX2}}, INDEX1),
            std::vector{
                CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, -50, ACCOUNT, 10, ACCOUNT3, 20, TXNID, 123)},
            fmt::format(
                R"({{
                    "obligations":{{
                        "JPY":"50"
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                ACCOUNT),
            R"("ledger_index" : "validated")"},
        NormalTestBundle{
            "ObligationOverflow",
            CreateOwnerDirLedgerObject({ripple::uint256{INDEX2}, ripple::uint256{INDEX2}}, INDEX1),
            std::vector{overflowState, overflowState},
            fmt::format(
                R"({{
                    "obligations":{{
                        "JPY":"9999999999999999e80"
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                ACCOUNT),
            R"("ledger_index" : "validated")"},
        NormalTestBundle{
            "HighID",
            CreateOwnerDirLedgerObject(
                {ripple::uint256{INDEX2}, ripple::uint256{INDEX2}, ripple::uint256{INDEX2}, ripple::uint256{INDEX2}},
                INDEX1),
            std::vector{
                // hotwallet
                CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 10, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123),
                // hotwallet
                CreateRippleStateLedgerObject(ACCOUNT, "CNY", ISSUER, 20, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123),
                CreateRippleStateLedgerObject(ACCOUNT, "EUR", ISSUER, 30, ACCOUNT3, 100, ACCOUNT, 200, TXNID, 123),
                CreateRippleStateLedgerObject(ACCOUNT, "JPY", ISSUER, -50, ACCOUNT3, 10, ACCOUNT, 20, TXNID, 123)},
            fmt::format(
                R"({{
                    "obligations":{{
                        "EUR":"30"
                    }},
                    "balances":{{
                        "{}":[
                            {{
                                "currency":"USD",
                                "value":"10"
                            }},
                            {{
                                "currency":"CNY",
                                "value":"20"
                            }}
                        ]
                    }},
                    "assets":{{
                        "{}":[
                            {{
                                "currency":"JPY",
                                "value":"50"
                            }}
                        ]
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                ACCOUNT2,
                ACCOUNT3,
                ACCOUNT),
            fmt::format(R"("hotwallet": "{}")", ACCOUNT2)},
        NormalTestBundle{
            "HotWalletArray",
            CreateOwnerDirLedgerObject(
                {ripple::uint256{INDEX2}, ripple::uint256{INDEX2}, ripple::uint256{INDEX2}}, INDEX1),
            std::vector{
                CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, -10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123),
                CreateRippleStateLedgerObject(ACCOUNT, "CNY", ISSUER, -20, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123),
                CreateRippleStateLedgerObject(ACCOUNT, "EUR", ISSUER, -30, ACCOUNT, 100, ACCOUNT3, 200, TXNID, 123)

            },
            fmt::format(
                R"({{
                    "balances":{{
                        "{}":[
                            {{
                                "currency":"EUR",
                                "value":"30"
                            }}
                        ],
                        "{}":[
                            {{
                                "currency":"USD",
                                "value":"10"
                            }},
                            {{
                                "currency":"CNY",
                                "value":"20"
                            }}
                        ]
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                ACCOUNT3,
                ACCOUNT2,
                ACCOUNT),
            fmt::format(R"("hotwallet": ["{}", "{}"])", ACCOUNT2, ACCOUNT3)},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    NormalPathTest,
    testing::ValuesIn(generateNormalPathTestBundles()),
    NormalPathTest::NameGenerator());
