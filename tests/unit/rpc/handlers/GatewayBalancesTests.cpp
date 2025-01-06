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
#include "rpc/handlers/GatewayBalances.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STObject.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kACCOUNT3 = "raHGBERMka3KZsfpTQUAtumxmvpqhFLyrk";
constexpr auto kISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTXN_ID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

}  // namespace

struct RPCGatewayBalancesHandlerTest : HandlerBaseTest {
    RPCGatewayBalancesHandlerTest()
    {
        backend_->setRange(10, 300);
    }
};

struct ParameterTestBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct ParameterTest : public RPCGatewayBalancesHandlerTest, public WithParamInterface<ParameterTestBundle> {};

TEST_P(ParameterTest, CheckError)
{
    auto bundle = GetParam();
    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(json::parse(bundle.testJson), Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), bundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), bundle.expectedErrorMessage);
    });
}

static auto
generateParameterTestBundles()
{
    return std::vector<ParameterTestBundle>{
        ParameterTestBundle{
            .testName = "AccountNotString",
            .testJson = R"({
                "account": 1213
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },
        ParameterTestBundle{
            .testName = "AccountMissing",
            .testJson = R"({
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },
        ParameterTestBundle{
            .testName = "AccountInvalid",
            .testJson = R"({
                "account": "1213"
            })",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": "meh"
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashInvalid",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": "meh"
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashNotString",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": 12
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringOrArray",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": 12
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletNotStringOrArray"
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringAccount",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": [12]
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed"
        },
        ParameterTestBundle{
            .testName = "WalletsInvalidAccount",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": ["12"]
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed"
        },
        ParameterTestBundle{
            .testName = "WalletInvalidAccount",
            .testJson = fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": "12"
                }})",
                kACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed"
        },
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    ParameterTest,
    testing::ValuesIn(generateParameterTestBundles()),
    tests::util::kNAME_GENERATOR
);

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaStringIndex)
{
    auto const seq = 123;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": "{}"
                }})",
                kACCOUNT,
                seq
            )),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaIntIndex)
{
    auto const seq = 123;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_index": {}
                }})",
                kACCOUNT,
                seq
            )),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "ledger_hash": "{}"
                }})",
                kACCOUNT,
                kLEDGER_HASH
            )),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, AccountNotFound)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    // return empty account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}"
                }})",
                kACCOUNT
            )),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCGatewayBalancesHandlerTest, InvalidHotWallet)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    // return valid account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return valid owner dir
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // create a valid line, balance is 0
    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 0, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123);
    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    "hotwallet": "{}"
                }})",
                kACCOUNT,
                kACCOUNT2
            )),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidHotWallet");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid hot wallet.");
    });
}

struct NormalTestBundle {
    std::string testName;
    ripple::STObject mockedDir;
    std::vector<ripple::STObject> mockedObjects;
    std::string expectedJson;
    std::string hotwallet;
};

struct NormalPathTest : public RPCGatewayBalancesHandlerTest, public WithParamInterface<NormalTestBundle> {};

TEST_P(NormalPathTest, CheckOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    // return valid account
    auto const accountKk = ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return valid owner dir
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(bundle.mockedDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    std::ranges::transform(
        bundle.mockedObjects,

        std::back_inserter(bbs),
        [](auto const& obj) { return obj.getSerializer().peekData(); }
    );
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            json::parse(fmt::format(
                R"({{
                    "account": "{}",
                    {}
                }})",
                kACCOUNT,
                bundle.hotwallet
            )),
            Context{yield}
        );
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), json::parse(bundle.expectedJson));
    });
}

static auto
generateNormalPathTestBundles()
{
    auto frozenState = createRippleStateLedgerObject("JPY", kISSUER, -50, kACCOUNT, 10, kACCOUNT3, 20, kTXN_ID, 123);
    frozenState.setFieldU32(ripple::sfFlags, ripple::lsfLowFreeze);

    auto overflowState = createRippleStateLedgerObject("JPY", kISSUER, 50, kACCOUNT, 10, kACCOUNT3, 20, kTXN_ID, 123);
    int64_t const min64 = -9922966390934554;
    overflowState.setFieldAmount(ripple::sfBalance, ripple::STAmount(getIssue("JPY", kISSUER), min64, 80));
    return std::vector<NormalTestBundle>{
        NormalTestBundle{
            .testName = "AllBranches",
            .mockedDir = createOwnerDirLedgerObject(
                {ripple::uint256{kINDEX2},
                 ripple::uint256{kINDEX2},
                 ripple::uint256{kINDEX2},
                 ripple::uint256{kINDEX2},
                 ripple::uint256{kINDEX2},
                 ripple::uint256{kINDEX2}},
                kINDEX1
            ),
            .mockedObjects =
                std::vector{
                    // hotwallet
                    createRippleStateLedgerObject("USD", kISSUER, -10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123),
                    // hotwallet
                    createRippleStateLedgerObject("CNY", kISSUER, -20, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123),
                    // positive balance -> asset
                    createRippleStateLedgerObject("EUR", kISSUER, 30, kACCOUNT, 100, kACCOUNT3, 200, kTXN_ID, 123),
                    // positive balance -> asset
                    createRippleStateLedgerObject("JPY", kISSUER, 40, kACCOUNT, 100, kACCOUNT3, 200, kTXN_ID, 123),
                    // obligation
                    createRippleStateLedgerObject("JPY", kISSUER, -50, kACCOUNT, 10, kACCOUNT3, 20, kTXN_ID, 123),
                    frozenState

                },
            .expectedJson = fmt::format(
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
                kACCOUNT2,
                kACCOUNT3,
                kACCOUNT3,
                kACCOUNT
            ),
            .hotwallet = fmt::format(R"("hotwallet": "{}")", kACCOUNT2)
        },
        NormalTestBundle{
            .testName = "NoHotwallet",
            .mockedDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1),
            .mockedObjects = std::vector{createRippleStateLedgerObject(
                "JPY", kISSUER, -50, kACCOUNT, 10, kACCOUNT3, 20, kTXN_ID, 123
            )},
            .expectedJson = fmt::format(
                R"({{
                    "obligations":{{
                        "JPY":"50"
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                kACCOUNT
            ),
            .hotwallet = R"("ledger_index" : "validated")"
        },
        NormalTestBundle{
            .testName = "ObligationOverflow",
            .mockedDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}}, kINDEX1),
            .mockedObjects = std::vector{overflowState, overflowState},
            .expectedJson = fmt::format(
                R"({{
                    "obligations":{{
                        "JPY":"9999999999999999e80"
                    }},
                    "account":"{}",
                    "ledger_index":300,
                    "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})",
                kACCOUNT
            ),
            .hotwallet = R"("ledger_index" : "validated")"
        },
        NormalTestBundle{
            .testName = "HighID",
            .mockedDir = createOwnerDirLedgerObject(
                {ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}
                },
                kINDEX1
            ),
            .mockedObjects =
                std::vector{
                    // hotwallet
                    createRippleStateLedgerObject("USD", kISSUER, 10, kACCOUNT2, 100, kACCOUNT, 200, kTXN_ID, 123),
                    // hotwallet
                    createRippleStateLedgerObject("CNY", kISSUER, 20, kACCOUNT2, 100, kACCOUNT, 200, kTXN_ID, 123),
                    createRippleStateLedgerObject("EUR", kISSUER, 30, kACCOUNT3, 100, kACCOUNT, 200, kTXN_ID, 123),
                    createRippleStateLedgerObject("JPY", kISSUER, -50, kACCOUNT3, 10, kACCOUNT, 20, kTXN_ID, 123)
                },
            .expectedJson = fmt::format(
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
                kACCOUNT2,
                kACCOUNT3,
                kACCOUNT
            ),
            .hotwallet = fmt::format(R"("hotwallet": "{}")", kACCOUNT2)
        },
        NormalTestBundle{
            .testName = "HotWalletArray",
            .mockedDir = createOwnerDirLedgerObject(
                {ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}}, kINDEX1
            ),
            .mockedObjects =
                std::vector{
                    createRippleStateLedgerObject("USD", kISSUER, -10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123),
                    createRippleStateLedgerObject("CNY", kISSUER, -20, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123),
                    createRippleStateLedgerObject("EUR", kISSUER, -30, kACCOUNT, 100, kACCOUNT3, 200, kTXN_ID, 123)

                },
            .expectedJson = fmt::format(
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
                kACCOUNT3,
                kACCOUNT2,
                kACCOUNT
            ),
            .hotwallet = fmt::format(R"("hotwallet": ["{}", "{}"])", kACCOUNT2, kACCOUNT3)
        },
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    NormalPathTest,
    testing::ValuesIn(generateNormalPathTestBundles()),
    tests::util::kNAME_GENERATOR
);
