#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountMPTokenIssuances.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kIssuanceIndeX1 = "A6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kIssuanceIndeX2 = "B6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

// unique values for issuance1
constexpr uint64_t kIssuancE1MaxAmount = 10000;
constexpr uint64_t kIssuancE1OutstandingAmount = 5000;
constexpr uint8_t kIssuancE1AssetScale = 2;
constexpr uint16_t kIssuancE1TransferFee = 10;

// unique values for issuance2
constexpr uint64_t kIssuancE2MaxAmount = 20000;
constexpr uint64_t kIssuancE2OutstandingAmount = 800;
constexpr uint64_t kIssuancE2LockedAmount = 100;
constexpr uint16_t kIssuancE2TransferFee = 5;
constexpr auto kIssuancE2Metadata = "test-meta";
constexpr auto kIssuancE2MetadataHex = "746573742D6D657461";
constexpr auto kIssuancE2DomainIdHex =
    "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

// define expected JSON for mpt issuances
auto const kIssuanceOuT1 = fmt::format(
    R"JSON({{
        "mpt_issuance_id": "{}",
        "issuer": "{}",
        "sequence": 1,
        "maximum_amount": {},
        "outstanding_amount": {},
        "asset_scale": {},
        "mpt_can_escrow": true,
        "mpt_can_trade": true,
        "mpt_require_auth": true,
        "mpt_can_transfer": true
    }})JSON",
    kIssuanceIndeX1,
    kAccount,
    kIssuancE1MaxAmount,
    kIssuancE1OutstandingAmount,
    kIssuancE1AssetScale
);

auto const kIssuanceOuT2 = fmt::format(
    R"JSON({{
        "mpt_issuance_id": "{}",
        "issuer": "{}",
        "sequence": 2,
        "maximum_amount": {},
        "outstanding_amount": {},
        "locked_amount": {},
        "transfer_fee": {},
        "mptoken_metadata": "{}",
        "domain_id": "{}",
        "mpt_can_lock": true,
        "mpt_locked": true,
        "mpt_can_clawback": true
    }})JSON",
    kIssuanceIndeX2,
    kAccount,
    kIssuancE2MaxAmount,
    kIssuancE2OutstandingAmount,
    kIssuancE2LockedAmount,
    kIssuancE2TransferFee,
    kIssuancE2MetadataHex,
    kIssuancE2DomainIdHex
);

}  // namespace

struct RPCAccountMPTokenIssuancesHandlerTest : HandlerBaseTest {
    RPCAccountMPTokenIssuancesHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

struct AccountMPTokenIssuancesParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct AccountMPTokenIssuancesParameterTest
    : RPCAccountMPTokenIssuancesHandlerTest,
      WithParamInterface<AccountMPTokenIssuancesParamTestCaseBundle> {};

// generate values for invalid params test
static auto
generateTestValuesForInvalidParamsTest()
{
    return std::vector<AccountMPTokenIssuancesParamTestCaseBundle>{
        {.testName = "NonHexLedgerHash",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}", "ledger_hash": "xxx" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashMalformed"},
        {.testName = "NonStringLedgerHash",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_hash": 123 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashNotString"},
        {.testName = "InvalidLedgerIndexString",
         .testJson = fmt::format(
             R"JSON({{ "account": "{}", "ledger_index": "notvalidated" }})JSON", kAccount
         ),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledgerIndexMalformed"},
        {.testName = "MarkerNotString",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "marker": 9 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "markerNotString"},
        {.testName = "InvalidMarkerContent",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}", "marker": "123invalid" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Malformed cursor."},
        {.testName = "AccountMissing",
         .testJson = R"JSON({ "limit": 10 })JSON",
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Required field 'account' missing"},
        {.testName = "AccountNotString",
         .testJson = R"JSON({ "account": 123 })JSON",
         .expectedError = "actMalformed",
         .expectedErrorMessage = "Account malformed."},
        {.testName = "AccountMalformed",
         .testJson = R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp" })JSON",
         .expectedError = "actMalformed",
         .expectedErrorMessage = "Account malformed."},
        {.testName = "LimitNotInteger",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": "t" }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitNegative",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": -1 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitZero",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": 0 }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitTypeInvalid",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": true }})JSON", kAccount),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."}
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesInvalidParamsGroup,
    AccountMPTokenIssuancesParameterTest,
    ValuesIn(generateTestValuesForInvalidParamsTest()),
    tests::util::kNameGenerator
);

// test invalid params bundle
TEST_P(AccountMPTokenIssuancesParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

// ledger not found via hash
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// ledger not found via string index
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "4"
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// ledger not found via int index
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": 4
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// ledger not found via hash (seq > max)
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LedgerSeqOutOfRangeByHash)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 31);
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// ledger not found via index (seq > max)
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LedgerSeqOutOfRangeByIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "31"
            }})JSON",
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// account not exist
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, NonExistAccount)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillOnce(Return(ledgerHeader));
    // fetch account object return empty
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

// fetch mptoken issuances via account successfully
TEST_F(RPCAccountMPTokenIssuancesHandlerTest, DefaultParameters)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // return non-empty account
    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return two mptoken issuance objects
    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kIssuanceIndeX1}, xrpl::uint256{kIssuanceIndeX2}}, kIssuanceIndeX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // mocking mptoken issuance ledger objects
    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kAccount,
            1,
            std::nullopt,
            xrpl::lsfMPTCanTrade | xrpl::lsfMPTRequireAuth | xrpl::lsfMPTCanTransfer |
                xrpl::lsfMPTCanEscrow,
            kIssuancE1OutstandingAmount,
            std::nullopt,
            kIssuancE1AssetScale,
            kIssuancE1MaxAmount
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kAccount,
            2,
            kIssuancE2Metadata,
            xrpl::lsfMPTLocked | xrpl::lsfMPTCanLock | xrpl::lsfMPTCanClawback,
            kIssuancE2OutstandingAmount,
            kIssuancE2TransferFee,
            std::nullopt,
            kIssuancE2MaxAmount,
            kIssuancE2LockedAmount,
            kIssuancE2DomainIdHex
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const expected = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mpt_issuances": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokenIssuancesHandler::kLimitDefault,
            kIssuanceOuT1,
            kIssuanceOuT2
        );
        auto const input =
            boost::json::parse(fmt::format(R"JSON({{"account": "{}"}})JSON", kAccount));
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};

        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(expected), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, UseLimit)
{
    constexpr int kLimit = 20;
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const indexes = std::vector<xrpl::uint256>(50, xrpl::uint256{kIssuanceIndeX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(50);
        for (int i = 0; i < 50; ++i) {
            v.push_back(createMptIssuanceObject(kAccount, i).getSerializer().peekData());
        }
        return v;
    }();

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIssuanceIndeX1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this, kLimit](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                kLimit
            )
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const resultJson = output.result->as_object();
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLimit);
        ASSERT_TRUE(resultJson.contains("marker"));
        EXPECT_THAT(boost::json::value_to<std::string>(resultJson.at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokenIssuancesHandler::kLimitMin - 1
            )
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(),
            AccountMPTokenIssuancesHandler::kLimitMin
        );
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokenIssuancesHandler::kLimitMax + 1
            )
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(),
            AccountMPTokenIssuancesHandler::kLimitMax
        );
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MarkerOutput)
{
    constexpr auto kNextPage = 99;
    constexpr auto kLimit = 15;
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const ownerDirKk = xrpl::keylet::ownerDir(account).key;
    auto const ownerDir2Kk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), kNextPage).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const indexes = std::vector<xrpl::uint256>(10, xrpl::uint256{kIssuanceIndeX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLimit);
        for (int i = 0; i < kLimit; ++i) {
            v.push_back(createMptIssuanceObject(kAccount, i).getSerializer().peekData());
        }
        return v;
    }();
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    // mock the first directory page
    xrpl::STObject ownerDir1 = createOwnerDirLedgerObject(indexes, kIssuanceIndeX1);
    ownerDir1.setFieldU64(xrpl::sfIndexNext, kNextPage);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir1.getSerializer().peekData()));

    // mock the second directory page
    xrpl::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kIssuanceIndeX2);
    ownerDir2.setFieldU64(xrpl::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, _, _))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    runSpawn([this, kLimit, kNextPage](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                kLimit
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        EXPECT_EQ(
            boost::json::value_to<std::string>(resultJson.at("marker")),
            fmt::format("{},{}", kIssuanceIndeX1, kNextPage)
        );
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLimit);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MarkerInput)
{
    constexpr auto kNextPage = 99;
    constexpr auto kLimit = 15;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const ownerDirKk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), kNextPage).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const indexes = std::vector<xrpl::uint256>(kLimit, xrpl::uint256{kIssuanceIndeX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLimit);
        for (int i = 0; i < kLimit; ++i) {
            v.push_back(createMptIssuanceObject(kAccount, i).getSerializer().peekData());
        }
        return v;
    }();

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIssuanceIndeX1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, kLimit, kNextPage](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {},
                    "marker": "{},{}"
                }})JSON",
                kAccount,
                kLimit,
                kIssuanceIndeX1,
                kNextPage
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const& resultJson = output.result->as_object();
        EXPECT_TRUE(resultJson.if_contains("marker") == nullptr);
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLimit - 1);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitLessThanMin)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kIssuanceIndeX1}, xrpl::uint256{kIssuanceIndeX2}}, kIssuanceIndeX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kAccount,
            1,
            std::nullopt,
            xrpl::lsfMPTCanTrade | xrpl::lsfMPTRequireAuth | xrpl::lsfMPTCanTransfer |
                xrpl::lsfMPTCanEscrow,
            kIssuancE1OutstandingAmount,
            std::nullopt,
            kIssuancE1AssetScale,
            kIssuancE1MaxAmount
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kAccount,
            2,
            kIssuancE2Metadata,
            xrpl::lsfMPTLocked | xrpl::lsfMPTCanLock | xrpl::lsfMPTCanClawback,
            kIssuancE2OutstandingAmount,
            kIssuancE2TransferFee,
            std::nullopt,
            kIssuancE2MaxAmount,
            kIssuancE2LockedAmount,
            kIssuancE2DomainIdHex
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokenIssuancesHandler::kLimitMin - 1
            )
        );

        auto const correctOutput = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mpt_issuances": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokenIssuancesHandler::kLimitMin,
            kIssuanceOuT1,
            kIssuanceOuT2
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitMoreThanMax)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kIssuanceIndeX1}, xrpl::uint256{kIssuanceIndeX2}}, kIssuanceIndeX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kAccount,
            1,
            std::nullopt,
            xrpl::lsfMPTCanTrade | xrpl::lsfMPTRequireAuth | xrpl::lsfMPTCanTransfer |
                xrpl::lsfMPTCanEscrow,
            kIssuancE1OutstandingAmount,
            std::nullopt,
            kIssuancE1AssetScale,
            kIssuancE1MaxAmount
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kAccount,
            2,
            kIssuancE2Metadata,
            xrpl::lsfMPTLocked | xrpl::lsfMPTCanLock | xrpl::lsfMPTCanClawback,
            kIssuancE2OutstandingAmount,
            kIssuancE2TransferFee,
            std::nullopt,
            kIssuancE2MaxAmount,
            kIssuancE2LockedAmount,
            kIssuancE2DomainIdHex
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kAccount,
                AccountMPTokenIssuancesHandler::kLimitMax + 1
            )
        );

        auto const correctOutput = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "mpt_issuances": [
                    {},
                    {}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            AccountMPTokenIssuancesHandler::kLimitMax,
            kIssuanceOuT1,
            kIssuanceOuT2
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, EmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject({}, kIssuanceIndeX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kAccount
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("mpt_issuances").as_array().size(), 0);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MutableFlags)
{
    uint32_t const mutableFlags1 = xrpl::lsmfMPTCanMutateCanLock |
        xrpl::lsmfMPTCanMutateRequireAuth | xrpl::lsmfMPTCanMutateCanEscrow |
        xrpl::lsmfMPTCanMutateCanTrade;

    uint32_t const mutableFlags2 = xrpl::lsmfMPTCanMutateCanTransfer |
        xrpl::lsmfMPTCanMutateCanClawback | xrpl::lsmfMPTCanMutateMetadata |
        xrpl::lsmfMPTCanMutateTransferFee;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kIssuanceIndeX1}, xrpl::uint256{kIssuanceIndeX2}}, kIssuanceIndeX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kAccount,
            3,
            std::nullopt,
            xrpl::lsfMPTCanTransfer,
            kIssuancE1OutstandingAmount,
            kIssuancE1TransferFee,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            mutableFlags1
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kAccount,
            5,
            kIssuancE2Metadata,
            xrpl::lsfMPTCanTransfer,
            kIssuancE2OutstandingAmount,
            kIssuancE2TransferFee,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            mutableFlags2
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kAccount
            )
        );

        auto const correctOutput = fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "ledger_index": 30,
                "validated": true,
                "limit": 200,
                "mpt_issuances": [
                    {{
                        "mpt_issuance_id": "{}",
                        "issuer": "{}",
                        "sequence": 3,
                        "outstanding_amount": {},
                        "transfer_fee": {},
                        "mpt_can_transfer": true,
                        "mpt_can_mutate_can_lock": true,
                        "mpt_can_mutate_require_auth": true,
                        "mpt_can_mutate_can_escrow": true,
                        "mpt_can_mutate_can_trade": true
                    }},
                    {{
                        "mpt_issuance_id": "{}",
                        "issuer": "{}",
                        "sequence": 5,
                        "outstanding_amount": {},
                        "transfer_fee": {},
                        "mptoken_metadata": "{}",
                        "mpt_can_transfer": true,
                        "mpt_can_mutate_can_transfer": true,
                        "mpt_can_mutate_can_clawback": true,
                        "mpt_can_mutate_metadata": true,
                        "mpt_can_mutate_transfer_fee": true
                    }}
                ]
            }})JSON",
            kAccount,
            kLedgerHash,
            kIssuanceIndeX1,
            kAccount,
            kIssuancE1OutstandingAmount,
            kIssuancE1TransferFee,
            kIssuanceIndeX2,
            kAccount,
            kIssuancE2OutstandingAmount,
            kIssuancE2TransferFee,
            kIssuancE2MetadataHex
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(correctOutput), *output.result);
    });
}

struct SingleFlagTest {
    std::string testName;
    uint32_t flag;
    std::string expectedJsonKey;
};

struct AccountMPTokenIssuancesImmutableFlagsTest : RPCAccountMPTokenIssuancesHandlerTest,
                                                   WithParamInterface<SingleFlagTest> {};

static auto
generateSingleFlagTests()
{
    return std::vector<SingleFlagTest>{
        {.testName = "Locked", .flag = xrpl::lsfMPTLocked, .expectedJsonKey = "mpt_locked"},
        {.testName = "CanLock", .flag = xrpl::lsfMPTCanLock, .expectedJsonKey = "mpt_can_lock"},
        {.testName = "RequireAuth",
         .flag = xrpl::lsfMPTRequireAuth,
         .expectedJsonKey = "mpt_require_auth"},
        {.testName = "CanEscrow",
         .flag = xrpl::lsfMPTCanEscrow,
         .expectedJsonKey = "mpt_can_escrow"},
        {.testName = "CanTrade", .flag = xrpl::lsfMPTCanTrade, .expectedJsonKey = "mpt_can_trade"},
        {.testName = "CanTransfer",
         .flag = xrpl::lsfMPTCanTransfer,
         .expectedJsonKey = "mpt_can_transfer"},
        {.testName = "CanClawback",
         .flag = xrpl::lsfMPTCanClawback,
         .expectedJsonKey = "mpt_can_clawback"},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesImmutableFlagsGroup,
    AccountMPTokenIssuancesImmutableFlagsTest,
    ValuesIn(generateSingleFlagTests()),
    tests::util::kNameGenerator
);

TEST_P(AccountMPTokenIssuancesImmutableFlagsTest, SingleFlag)
{
    auto const testParams = GetParam();

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIssuanceIndeX1}}, kIssuanceIndeX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs =
        std::vector<Blob>{createMptIssuanceObject(kAccount, 1, std::nullopt, testParams.flag, 0)
                              .getSerializer()
                              .peekData()};

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, &testParams](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kAccount
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        auto const& issuances = resultJson.at("mpt_issuances").as_array();
        ASSERT_EQ(issuances.size(), 1);

        auto const& issuanceJson = issuances[0].as_object();
        EXPECT_TRUE(issuanceJson.contains(testParams.expectedJsonKey));
        EXPECT_EQ(issuanceJson.at(testParams.expectedJsonKey), true);
    });
}

struct SingleMutableFlagTest {
    std::string testName;
    uint32_t mutableFlag;
    std::string expectedJsonKey;
};

struct AccountMPTokenIssuancesMutableFlagsTest : RPCAccountMPTokenIssuancesHandlerTest,
                                                 WithParamInterface<SingleMutableFlagTest> {};

static auto
generateSingleMutableFlagTests()
{
    return std::vector<SingleMutableFlagTest>{
        {.testName = "CanMutateCanLock",
         .mutableFlag = xrpl::lsmfMPTCanMutateCanLock,
         .expectedJsonKey = "mpt_can_mutate_can_lock"},
        {.testName = "CanMutateRequireAuth",
         .mutableFlag = xrpl::lsmfMPTCanMutateRequireAuth,
         .expectedJsonKey = "mpt_can_mutate_require_auth"},
        {.testName = "CanMutateCanEscrow",
         .mutableFlag = xrpl::lsmfMPTCanMutateCanEscrow,
         .expectedJsonKey = "mpt_can_mutate_can_escrow"},
        {.testName = "CanMutateCanTrade",
         .mutableFlag = xrpl::lsmfMPTCanMutateCanTrade,
         .expectedJsonKey = "mpt_can_mutate_can_trade"},
        {.testName = "CanMutateCanTransfer",
         .mutableFlag = xrpl::lsmfMPTCanMutateCanTransfer,
         .expectedJsonKey = "mpt_can_mutate_can_transfer"},
        {.testName = "CanMutateCanClawback",
         .mutableFlag = xrpl::lsmfMPTCanMutateCanClawback,
         .expectedJsonKey = "mpt_can_mutate_can_clawback"},
        {.testName = "CanMutateMetadata",
         .mutableFlag = xrpl::lsmfMPTCanMutateMetadata,
         .expectedJsonKey = "mpt_can_mutate_metadata"},
        {.testName = "CanMutateTransferFee",
         .mutableFlag = xrpl::lsmfMPTCanMutateTransferFee,
         .expectedJsonKey = "mpt_can_mutate_transfer_fee"},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesMutableFlagsGroup,
    AccountMPTokenIssuancesMutableFlagsTest,
    ValuesIn(generateSingleMutableFlagTests()),
    tests::util::kNameGenerator
);

TEST_P(AccountMPTokenIssuancesMutableFlagsTest, SingleMutableFlag)
{
    auto const testParams = GetParam();

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kAccount);
    auto const accountKk = xrpl::keylet::account(account).key;
    auto const owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    xrpl::STObject const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIssuanceIndeX1}}, kIssuanceIndeX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{createMptIssuanceObject(
                                           kAccount,
                                           1,
                                           std::nullopt,
                                           0,
                                           0,
                                           std::nullopt,
                                           std::nullopt,
                                           std::nullopt,
                                           std::nullopt,
                                           std::nullopt,
                                           testParams.mutableFlag
    )
                                           .getSerializer()
                                           .peekData()};

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, &testParams](auto yield) {
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kAccount
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        auto const& issuances = resultJson.at("mpt_issuances").as_array();
        ASSERT_EQ(issuances.size(), 1);

        auto const& issuanceJson = issuances[0].as_object();
        EXPECT_TRUE(issuanceJson.contains(testParams.expectedJsonKey));
        EXPECT_EQ(issuanceJson.at(testParams.expectedJsonKey), true);
    });
}
