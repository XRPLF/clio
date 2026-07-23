#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/DepositAuthorized.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <chrono>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";
constexpr auto kCredentialHash = "F245428267E6177AEEFDD4FEA3533285712A4B1091CF82A7EA7BC39A62C3FB1A";
constexpr std::string_view kCredentialType = "credType";

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;

}  // namespace

using namespace rpc;
using namespace data;
using namespace testing;

struct RPCDepositAuthorizedTest : HandlerBaseTest {
    RPCDepositAuthorizedTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
    }
};

struct DepositAuthorizedTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct DepositAuthorizedParameterTest : RPCDepositAuthorizedTest,
                                        WithParamInterface<DepositAuthorizedTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<DepositAuthorizedTestCaseBundle>{
        {
            .testName = "SourceAccountMissing",
            .testJson = R"JSON({
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'source_account' missing",
        },
        {
            .testName = "SourceAccountMalformed",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "source_accountMalformed",
        },
        {
            .testName = "SourceAccountNotString",
            .testJson = R"JSON({
                "source_account": 1234,
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "source_accountNotString",
        },
        {
            .testName = "DestinationAccountMissing",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'destination_account' missing",
        },
        {
            .testName = "DestinationAccountMalformed",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "destination_accountMalformed",
        },
        {
            .testName = "DestinationAccountNotString",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": 1234,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "destination_accountNotString",
        },
        {
            .testName = "LedgerHashInvalid",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "x"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed",
        },
        {
            .testName = "LedgerHashNotString",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString",
        },
        {
            .testName = "LedgerIndexNotInt",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "x"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed",
        },
        {
            .testName = "CredentialsNotArray",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "credentials": "x"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "CredentialsNotStringsInArray",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": [123]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Item is not a valid uint256 type.",
        },
        {
            .testName = "CredentialsNotHexedStringInArray",
            .testJson = R"JSON({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": ["234", "432"]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Item is not a valid uint256 type.",
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCDepositAuthorizedGroup,
    DepositAuthorizedParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(DepositAuthorizedParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "source_account": "{}",
                    "destination_account": "{}",
                    "ledger_index": {}
                }})JSON",
                kAccount,
                kAccount2,
                kRangeMax
            )
        );

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "source_account": "{}",
                    "destination_account": "{}",
                    "ledger_index": "{}"
                }})JSON",
                kAccount,
                kAccount2,
                kRangeMax
            )
        );

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "source_account": "{}",
                    "destination_account": "{}",
                    "ledger_hash": "{}"
                }})JSON",
                kAccount,
                kAccount2,
                kLedgerHash
            )
        );

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, SourceAccountDoesNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "srcActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "source_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, DestinationAccountDoesNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const accountRoot = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(std::optional<Blob>{}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "dstActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "destination_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, AccountsAreEqual)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const accountRoot = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsNoDepositAuthFlag)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root = createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex2, 2);

    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsFalse)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": false,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::nullopt));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsTrue)
{
    static constexpr auto kExpectedOut =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialAcceptedAndNotExpiredReturnsTrue)
{
    static auto const kExpectedOut = fmt::format(
        R"JSON({{
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "credentials": ["{}"]
        }})JSON",
        kCredentialHash  // CREDENTIALHASH should match credentialIndex
    );

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);
    auto const credential = createCredentialObject(kAccount, kAccount2, kCredentialType);
    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount),
                                     getAccountIdWithString(kAccount2),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(4);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": ["{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            xrpl::strHex(credentialIndex)
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialNotAuthorizedReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);
    auto const credential = createCredentialObject(kAccount, kAccount2, kCredentialType, false);
    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount),
                                     getAccountIdWithString(kAccount2),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": ["{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            xrpl::strHex(credentialIndex)
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials aren't accepted");
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialExpiredReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30, 100);

    // set parent close time to 500 seconds
    ledgerHeader.parentCloseTime = xrpl::NetClock::time_point{std::chrono::seconds{500}};

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);

    // credential expire time is 23 seconds, so credential will fail
    auto const expiredCredential =
        createCredentialObject(kAccount, kAccount2, kCredentialType, true, 23);

    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount),
                                     getAccountIdWithString(kAccount2),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(expiredCredential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": ["{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            xrpl::strHex(credentialIndex)
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials are expired");
    });
}

TEST_F(RPCDepositAuthorizedTest, DuplicateCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);
    auto const credential = createCredentialObject(kAccount, kAccount2, kCredentialType);
    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount),
                                     getAccountIdWithString(kAccount2),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": ["{}", "{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            xrpl::strHex(credentialIndex),
            xrpl::strHex(credentialIndex)
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "duplicates in credentials.");
    });
}

TEST_F(RPCDepositAuthorizedTest, NoElementsInCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": []
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array has no elements.");
    });
}

TEST_F(RPCDepositAuthorizedTest, MoreThanMaxNumberOfCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);
    auto const credential = createCredentialObject(kAccount, kAccount2, kCredentialType);
    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount),
                                     getAccountIdWithString(kAccount2),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<std::string> credentials(9, xrpl::strHex(credentialIndex));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": [{}]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            fmt::join(
                credentials | std::views::transform([](std::string const& cred) {
                    return fmt::format("\"{}\"", cred);
                }),
                ", "
            )
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array too long.");
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentSubjectAccountForCredentialReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2);
    auto const account2Root =
        createAccountRootObject(kAccount2, xrpl::lsfDepositAuth, 2, 200, 2, kIndex2, 2);

    // reverse the subject and issuer account. Now subject is Account2
    auto const credential = createCredentialObject(kAccount2, kAccount, kCredentialType);
    auto const credentialIndex = xrpl::keylet::credential(
                                     getAccountIdWithString(kAccount2),
                                     getAccountIdWithString(kAccount),
                                     xrpl::Slice(kCredentialType.data(), kCredentialType.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, _, _)
    )
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, _, _)
    )
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "source_account": "{}",
                "destination_account": "{}",
                "ledger_hash": "{}",
                "credentials": ["{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kLedgerHash,
            xrpl::strHex(credentialIndex)
        )
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(
            err.at("error_message").as_string(), "credentials don't belong to the root account"
        );
    });
}
