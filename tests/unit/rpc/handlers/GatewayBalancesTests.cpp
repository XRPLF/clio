#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/GatewayBalances.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
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
using namespace data;
using namespace testing;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kAccount3 = "raHGBERMka3KZsfpTQUAtumxmvpqhFLyrk";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTxnId = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";
constexpr auto kApiVersion = 2;

struct ParameterTestBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
    std::uint32_t apiVersion = 1u;
};
}  // namespace

struct RPCGatewayBalancesHandlerTest : HandlerBaseTest {
    RPCGatewayBalancesHandlerTest()
    {
        backend_->setRange(10, 300);
    }
};

struct ParameterTest : public RPCGatewayBalancesHandlerTest,
                       public WithParamInterface<ParameterTestBundle> {};

TEST_P(ParameterTest, CheckError)
{
    auto bundle = GetParam();
    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(bundle.testJson),
            Context{.yield = yield, .apiVersion = bundle.apiVersion}
        );
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
            .testJson = R"JSON({
                "account": 1213
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },
        ParameterTestBundle{
            .testName = "AccountMissing",
            .testJson = R"JSON({
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },
        ParameterTestBundle{
            .testName = "AccountInvalid",
            .testJson = R"JSON({
                "account": "1213"
            })JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "ledger_index": "meh"
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "ledger_hash": "meh"
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "ledger_hash": 12
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringOrArrayV1",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": 12
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidHotWallet",
            .expectedErrorMessage = "hotwalletNotStringOrArray"
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringAccountV1",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": [12]
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidHotWallet",
            .expectedErrorMessage = "hotwalletMalformed"
        },
        ParameterTestBundle{
            .testName = "WalletsInvalidAccountV1",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": ["12"]
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidHotWallet",
            .expectedErrorMessage = "hotwalletMalformed"
        },
        ParameterTestBundle{
            .testName = "WalletInvalidAccountV1",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": "12"
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidHotWallet",
            .expectedErrorMessage = "hotwalletMalformed"
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringOrArrayV2",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": 12
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletNotStringOrArray",
            .apiVersion = kApiVersion
        },
        ParameterTestBundle{
            .testName = "WalletsNotStringAccountV2",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": [12]
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed",
            .apiVersion = kApiVersion
        },
        ParameterTestBundle{
            .testName = "WalletsInvalidAccountV2",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": ["12"]
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed",
            .apiVersion = kApiVersion
        },
        ParameterTestBundle{
            .testName = "WalletInvalidAccountV2",
            .testJson = fmt::format(
                R"JSON({{
                    "account": "{}",
                    "hotwallet": "12"
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "hotwalletMalformed",
            .apiVersion = kApiVersion
        },
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    ParameterTest,
    testing::ValuesIn(generateParameterTestBundles()),
    tests::util::kNameGenerator
);

TEST_F(RPCGatewayBalancesHandlerTest, LedgerNotFoundViaStringIndex)
{
    auto const seq = 123;

    EXPECT_CALL(*backend_, fetchLedgerBySequence(seq, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}",
                        "ledger_index": "{}"
                    }})JSON",
                    kAccount,
                    seq
                )
            ),
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

    EXPECT_CALL(*backend_, fetchLedgerBySequence(seq, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}",
                        "ledger_index": {}
                    }})JSON",
                    kAccount,
                    seq
                )
            ),
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
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}",
                        "ledger_hash": "{}"
                    }})JSON",
                    kAccount,
                    kLedgerHash
                )
            ),
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

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillOnce(Return(ledgerHeader));

    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _))
        .WillOnce(Return(std::optional<Blob>{}));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}"
                    }})JSON",
                    kAccount
                )
            ),
            Context{yield}
        );
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

struct NormalTestBundle {
    std::string testName;
    xrpl::STObject mockedDir;
    std::vector<xrpl::STObject> mockedObjects;
    std::string expectedJson;
    std::string hotwallet;
};

struct NormalPathTest : public RPCGatewayBalancesHandlerTest,
                        public WithParamInterface<NormalTestBundle> {};

TEST_P(NormalPathTest, CheckOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;

    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillOnce(Return(ledgerHeader));

    // return valid account
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    // return valid owner dir
    auto const ownerDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillOnce(Return(bundle.mockedDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    std::ranges::transform(bundle.mockedObjects, std::back_inserter(bbs), [](auto const& obj) {
        return obj.getSerializer().peekData();
    });
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}",
                        {}
                    }})JSON",
                    kAccount,
                    bundle.hotwallet
                )
            ),
            Context{yield}
        );
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(bundle.expectedJson));
    });
}

static auto
generateNormalPathTestBundles()
{
    auto frozenState = createRippleStateLedgerObject(
        "JPY", kIssuer, -50, kAccount, 10, kAccount3, 20, kTxnId, 123
    );
    frozenState.setFieldU32(xrpl::sfFlags, xrpl::lsfLowFreeze);

    auto overflowState =
        createRippleStateLedgerObject("JPY", kIssuer, 50, kAccount, 10, kAccount3, 20, kTxnId, 123);
    int64_t const min64 = -9922966390934554;
    overflowState.setFieldAmount(
        xrpl::sfBalance, xrpl::STAmount(getIssue("JPY", kIssuer), min64, 80)
    );
    return std::vector<NormalTestBundle>{
        NormalTestBundle{
            .testName = "AllBranches",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2}},
                kIndex1
            ),
            .mockedObjects =
                std::vector{
                    // hotwallet
                    createRippleStateLedgerObject(
                        "USD", kIssuer, -10, kAccount, 100, kAccount2, 200, kTxnId, 123
                    ),
                    // hotwallet
                    createRippleStateLedgerObject(
                        "CNY", kIssuer, -20, kAccount, 100, kAccount2, 200, kTxnId, 123
                    ),
                    // positive balance -> asset
                    createRippleStateLedgerObject(
                        "EUR", kIssuer, 30, kAccount, 100, kAccount3, 200, kTxnId, 123
                    ),
                    // positive balance -> asset
                    createRippleStateLedgerObject(
                        "JPY", kIssuer, 40, kAccount, 100, kAccount3, 200, kTxnId, 123
                    ),
                    // obligation
                    createRippleStateLedgerObject(
                        "JPY", kIssuer, -50, kAccount, 10, kAccount3, 20, kTxnId, 123
                    ),
                    frozenState

                },
            .expectedJson = fmt::format(
                R"JSON({{
                    "obligations": {{
                        "JPY": "50"
                    }},
                    "balances": {{
                        "{}": [
                            {{
                                "currency": "USD",
                                "value": "10"
                            }},
                            {{
                                "currency": "CNY",
                                "value": "20"
                            }}
                        ]
                    }},
                    "frozen_balances": {{
                        "{}": [
                            {{
                                "currency": "JPY",
                                "value": "50"
                            }}
                        ]
                    }},
                    "assets": {{
                        "{}": [
                            {{
                                "currency": "EUR",
                                "value": "30"
                            }},
                            {{
                                "currency": "JPY",
                                "value": "40"
                            }}
                        ]
                    }},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})JSON",
                kAccount2,
                kAccount3,
                kAccount3,
                kAccount
            ),
            .hotwallet = fmt::format(R"("hotwallet": "{}")", kAccount2)
        },
        NormalTestBundle{
            .testName = "NoHotwallet",
            .mockedDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1),
            .mockedObjects = std::vector{createRippleStateLedgerObject(
                "JPY", kIssuer, -50, kAccount, 10, kAccount3, 20, kTxnId, 123
            )},
            .expectedJson = fmt::format(
                R"JSON({{
                    "obligations": {{
                        "JPY": "50"
                    }},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})JSON",
                kAccount
            ),
            .hotwallet = R"("ledger_index" : "validated")"
        },
        NormalTestBundle{
            .testName = "ObligationOverflow",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
            ),
            .mockedObjects = std::vector{overflowState, overflowState},
            .expectedJson = fmt::format(
                R"JSON({{
                    "obligations": {{
                        "JPY": "9999999999999999e80"
                    }},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})JSON",
                kAccount
            ),
            .hotwallet = R"("ledger_index" : "validated")"
        },
        NormalTestBundle{
            .testName = "HighID",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2},
                 xrpl::uint256{kIndex2}},
                kIndex1
            ),
            .mockedObjects =
                std::vector{
                    // hotwallet
                    createRippleStateLedgerObject(
                        "USD", kIssuer, 10, kAccount2, 100, kAccount, 200, kTxnId, 123
                    ),
                    // hotwallet
                    createRippleStateLedgerObject(
                        "CNY", kIssuer, 20, kAccount2, 100, kAccount, 200, kTxnId, 123
                    ),
                    createRippleStateLedgerObject(
                        "EUR", kIssuer, 30, kAccount3, 100, kAccount, 200, kTxnId, 123
                    ),
                    createRippleStateLedgerObject(
                        "JPY", kIssuer, -50, kAccount3, 10, kAccount, 20, kTxnId, 123
                    )
                },
            .expectedJson = fmt::format(
                R"JSON({{
                    "obligations": {{
                        "EUR": "30"
                    }},
                    "balances": {{
                        "{}": [
                            {{
                                "currency": "USD",
                                "value": "10"
                            }},
                            {{
                                "currency": "CNY",
                                "value": "20"
                            }}
                        ]
                    }},
                    "assets": {{
                        "{}": [
                            {{
                                "currency": "JPY",
                                "value": "50"
                            }}
                        ]
                    }},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})JSON",
                kAccount2,
                kAccount3,
                kAccount
            ),
            .hotwallet = fmt::format(R"("hotwallet": "{}")", kAccount2)
        },
        NormalTestBundle{
            .testName = "HotWalletArray",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
            ),
            .mockedObjects =
                std::vector{
                    createRippleStateLedgerObject(
                        "USD", kIssuer, -10, kAccount, 100, kAccount2, 200, kTxnId, 123
                    ),
                    createRippleStateLedgerObject(
                        "CNY", kIssuer, -20, kAccount, 100, kAccount2, 200, kTxnId, 123
                    ),
                    createRippleStateLedgerObject(
                        "EUR", kIssuer, -30, kAccount, 100, kAccount3, 200, kTxnId, 123
                    )

                },
            .expectedJson = fmt::format(
                R"JSON({{
                    "balances": {{
                        "{}": [
                            {{
                                "currency": "EUR",
                                "value": "30"
                            }}
                        ],
                        "{}": [
                            {{
                                "currency": "USD",
                                "value": "10"
                            }},
                            {{
                                "currency": "CNY",
                                "value": "20"
                            }}
                        ]
                    }},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
                }})JSON",
                kAccount3,
                kAccount2,
                kAccount
            ),
            .hotwallet = fmt::format(R"("hotwallet": ["{}", "{}"])", kAccount2, kAccount3)
        },
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    NormalPathTest,
    testing::ValuesIn(generateNormalPathTestBundles()),
    tests::util::kNameGenerator
);

struct EscrowTestBundle {
    std::string testName;
    xrpl::STObject mockedDir;
    std::vector<xrpl::STObject> mockedObjects;
    std::string expectedJson;
};

struct EscrowTest : public RPCGatewayBalancesHandlerTest,
                    public WithParamInterface<EscrowTestBundle> {};

TEST_P(EscrowTest, CheckEscrowOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillOnce(Return(ledgerHeader));

    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, seq, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillOnce(Return(bundle.mockedDir.getSerializer().peekData()));

    std::vector<Blob> bbs;
    std::ranges::transform(bundle.mockedObjects, std::back_inserter(bbs), [](auto const& obj) {
        return obj.getSerializer().peekData();
    });
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    auto const handler = AnyHandler{GatewayBalancesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(
            boost::json::parse(
                fmt::format(
                    R"JSON({{
                        "account": "{}"
                    }})JSON",
                    kAccount
                )
            ),
            Context{yield}
        );
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(bundle.expectedJson));
    });
}

static auto
generateEscrowTestBundles()
{
    // Escrow with 100 XRP
    auto escrow1 = createEscrowLedgerObject(kAccount, kAccount2);
    escrow1.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));

    // Escrow with 200 XRP
    auto escrow2 = createEscrowLedgerObject(kAccount, kAccount3);
    escrow2.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(200, false));

    // Escrow with a non-XRP currency
    auto escrow3 = createEscrowLedgerObject(kAccount, kAccount2);
    escrow3.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(getIssue("USD", kIssuer), 50));

    return std::vector<EscrowTestBundle>{
        EscrowTestBundle{
            .testName = "SingleEscrowXRP",
            .mockedDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1),
            .mockedObjects = std::vector{escrow1},
            .expectedJson = fmt::format(
                R"JSON({{
                    "locked": {{"XRP": "100"}},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "{}"
                }})JSON",
                kAccount,
                kLedgerHash
            )
        },
        EscrowTestBundle{
            .testName = "MultipleEscrowXRP",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
            ),
            .mockedObjects = std::vector{escrow1, escrow2},
            .expectedJson = fmt::format(
                R"JSON({{
                    "locked": {{"XRP": "300"}},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "{}"
                }})JSON",
                kAccount,
                kLedgerHash
            )
        },
        EscrowTestBundle{
            .testName = "EscrowNonXRP",
            .mockedDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1),
            .mockedObjects = std::vector{escrow3},
            .expectedJson = fmt::format(
                R"JSON({{
                    "locked": {{"USD": "50"}},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "{}"
                }})JSON",
                kAccount,
                kLedgerHash
            )
        },
        EscrowTestBundle{
            .testName = "EscrowMixedCurrencies",
            .mockedDir = createOwnerDirLedgerObject(
                {xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
            ),
            .mockedObjects = std::vector{escrow1, escrow2, escrow3},
            .expectedJson = fmt::format(
                R"JSON({{
                    "locked": {{"XRP": "300", "USD": "50"}},
                    "account": "{}",
                    "ledger_index": 300,
                    "ledger_hash": "{}"
                }})JSON",
                kAccount,
                kLedgerHash
            )
        }
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCGatewayBalancesHandler,
    EscrowTest,
    testing::ValuesIn(generateEscrowTestBundles()),
    tests::util::kNameGenerator
);
