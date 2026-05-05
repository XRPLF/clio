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
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kISSUANCE_INDEX1 =
    "A6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kISSUANCE_INDEX2 =
    "B6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

// unique values for issuance1
constexpr uint64_t kISSUANCE1_MAX_AMOUNT = 10000;
constexpr uint64_t kISSUANCE1_OUTSTANDING_AMOUNT = 5000;
constexpr uint8_t kISSUANCE1_ASSET_SCALE = 2;
constexpr uint16_t kISSUANCE1_TRANSFER_FEE = 10;

// unique values for issuance2
constexpr uint64_t kISSUANCE2_MAX_AMOUNT = 20000;
constexpr uint64_t kISSUANCE2_OUTSTANDING_AMOUNT = 800;
constexpr uint64_t kISSUANCE2_LOCKED_AMOUNT = 100;
constexpr uint16_t kISSUANCE2_TRANSFER_FEE = 5;
constexpr auto kISSUANCE2_METADATA = "test-meta";
constexpr auto kISSUANCE2_METADATA_HEX = "746573742D6D657461";
constexpr auto kISSUANCE2_DOMAIN_ID_HEX =
    "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

// define expected JSON for mpt issuances
auto const kISSUANCE_OUT1 = fmt::format(
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
    kISSUANCE_INDEX1,
    kACCOUNT,
    kISSUANCE1_MAX_AMOUNT,
    kISSUANCE1_OUTSTANDING_AMOUNT,
    kISSUANCE1_ASSET_SCALE
);

auto const kISSUANCE_OUT2 = fmt::format(
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
    kISSUANCE_INDEX2,
    kACCOUNT,
    kISSUANCE2_MAX_AMOUNT,
    kISSUANCE2_OUTSTANDING_AMOUNT,
    kISSUANCE2_LOCKED_AMOUNT,
    kISSUANCE2_TRANSFER_FEE,
    kISSUANCE2_METADATA_HEX,
    kISSUANCE2_DOMAIN_ID_HEX
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
             fmt::format(R"JSON({{ "account": "{}", "ledger_hash": "xxx" }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashMalformed"},
        {.testName = "NonStringLedgerHash",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "ledger_hash": 123 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledger_hashNotString"},
        {.testName = "InvalidLedgerIndexString",
         .testJson = fmt::format(
             R"JSON({{ "account": "{}", "ledger_index": "notvalidated" }})JSON", kACCOUNT
         ),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "ledgerIndexMalformed"},
        {.testName = "MarkerNotString",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "marker": 9 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "markerNotString"},
        {.testName = "InvalidMarkerContent",
         .testJson =
             fmt::format(R"JSON({{ "account": "{}", "marker": "123invalid" }})JSON", kACCOUNT),
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
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": "t" }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitNegative",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": -1 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitZero",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": 0 }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."},
        {.testName = "LimitTypeInvalid",
         .testJson = fmt::format(R"JSON({{ "account": "{}", "limit": true }})JSON", kACCOUNT),
         .expectedError = "invalidParams",
         .expectedErrorMessage = "Invalid parameters."}
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesInvalidParamsGroup,
    AccountMPTokenIssuancesParameterTest,
    ValuesIn(generateTestValuesForInvalidParamsTest()),
    tests::util::kNAME_GENERATOR
);

// test invalid params bundle
TEST_P(AccountMPTokenIssuancesParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
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
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
        .WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "4"
            }})JSON",
            kACCOUNT
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
        .WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": 4
            }})JSON",
            kACCOUNT
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 31);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillOnce(Return(ledgerHeader));
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "31"
            }})JSON",
            kACCOUNT
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillOnce(Return(ledgerHeader));
    // fetch account object return empty
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(Return(std::optional<Blob>{}));

    auto const input = json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kACCOUNT,
            kLEDGER_HASH
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
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // return non-empty account
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return two mptoken issuance objects
    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // mocking mptoken issuance ledger objects
    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kACCOUNT,
            1,
            std::nullopt,
            ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer |
                ripple::lsfMPTCanEscrow,
            kISSUANCE1_OUTSTANDING_AMOUNT,
            std::nullopt,
            kISSUANCE1_ASSET_SCALE,
            kISSUANCE1_MAX_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kACCOUNT,
            2,
            kISSUANCE2_METADATA,
            ripple::lsfMPTLocked | ripple::lsfMPTCanLock | ripple::lsfMPTCanClawback,
            kISSUANCE2_OUTSTANDING_AMOUNT,
            kISSUANCE2_TRANSFER_FEE,
            std::nullopt,
            kISSUANCE2_MAX_AMOUNT,
            kISSUANCE2_LOCKED_AMOUNT,
            kISSUANCE2_DOMAIN_ID_HEX
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokenIssuancesHandler::kLIMIT_DEFAULT,
            kISSUANCE_OUT1,
            kISSUANCE_OUT2
        );
        auto const input = json::parse(fmt::format(R"JSON({{"account": "{}"}})JSON", kACCOUNT));
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};

        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(expected), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, UseLimit)
{
    constexpr int kLIMIT = 20;
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const indexes = std::vector<ripple::uint256>(50, ripple::uint256{kISSUANCE_INDEX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(50);
        for (int i = 0; i < 50; ++i) {
            v.push_back(createMptIssuanceObject(kACCOUNT, i).getSerializer().peekData());
        }
        return v;
    }();

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kISSUANCE_INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this, kLIMIT](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                kLIMIT
            )
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const resultJson = output.result->as_object();
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLIMIT);
        ASSERT_TRUE(resultJson.contains("marker"));
        EXPECT_THAT(boost::json::value_to<std::string>(resultJson.at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokenIssuancesHandler::kLIMIT_MIN - 1
            )
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(),
            AccountMPTokenIssuancesHandler::kLIMIT_MIN
        );
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{backend_}};
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokenIssuancesHandler::kLIMIT_MAX + 1
            )
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("limit").as_uint64(),
            AccountMPTokenIssuancesHandler::kLIMIT_MAX
        );
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MarkerOutput)
{
    constexpr auto kNEXT_PAGE = 99;
    constexpr auto kLIMIT = 15;
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const ownerDirKk = ripple::keylet::ownerDir(account).key;
    auto const ownerDir2Kk =
        ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const indexes = std::vector<ripple::uint256>(10, ripple::uint256{kISSUANCE_INDEX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLIMIT);
        for (int i = 0; i < kLIMIT; ++i) {
            v.push_back(createMptIssuanceObject(kACCOUNT, i).getSerializer().peekData());
        }
        return v;
    }();
    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    // mock the first directory page
    ripple::STObject ownerDir1 = createOwnerDirLedgerObject(indexes, kISSUANCE_INDEX1);
    ownerDir1.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir1.getSerializer().peekData()));

    // mock the second directory page
    ripple::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kISSUANCE_INDEX2);
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, _, _))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    runSpawn([this, kLIMIT, kNEXT_PAGE](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                kLIMIT
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = output.result->as_object();
        EXPECT_EQ(
            boost::json::value_to<std::string>(resultJson.at("marker")),
            fmt::format("{},{}", kISSUANCE_INDEX1, kNEXT_PAGE)
        );
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLIMIT);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MarkerInput)
{
    constexpr auto kNEXT_PAGE = 99;
    constexpr auto kLIMIT = 15;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const ownerDirKk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const indexes = std::vector<ripple::uint256>(kLIMIT, ripple::uint256{kISSUANCE_INDEX1});
    auto const bbs = [&]() {
        std::vector<Blob> v;
        v.reserve(kLIMIT);
        for (int i = 0; i < kLIMIT; ++i) {
            v.push_back(createMptIssuanceObject(kACCOUNT, i).getSerializer().peekData());
        }
        return v;
    }();

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kISSUANCE_INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, _, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, kLIMIT, kNEXT_PAGE](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {},
                    "marker": "{},{}"
                }})JSON",
                kACCOUNT,
                kLIMIT,
                kISSUANCE_INDEX1,
                kNEXT_PAGE
            )
        );
        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const& resultJson = output.result->as_object();
        EXPECT_TRUE(resultJson.if_contains("marker") == nullptr);
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLIMIT - 1);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitLessThanMin)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kACCOUNT,
            1,
            std::nullopt,
            ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer |
                ripple::lsfMPTCanEscrow,
            kISSUANCE1_OUTSTANDING_AMOUNT,
            std::nullopt,
            kISSUANCE1_ASSET_SCALE,
            kISSUANCE1_MAX_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kACCOUNT,
            2,
            kISSUANCE2_METADATA,
            ripple::lsfMPTLocked | ripple::lsfMPTCanLock | ripple::lsfMPTCanClawback,
            kISSUANCE2_OUTSTANDING_AMOUNT,
            kISSUANCE2_TRANSFER_FEE,
            std::nullopt,
            kISSUANCE2_MAX_AMOUNT,
            kISSUANCE2_LOCKED_AMOUNT,
            kISSUANCE2_DOMAIN_ID_HEX
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokenIssuancesHandler::kLIMIT_MIN - 1
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokenIssuancesHandler::kLIMIT_MIN,
            kISSUANCE_OUT1,
            kISSUANCE_OUT2
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitMoreThanMax)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kACCOUNT,
            1,
            std::nullopt,
            ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer |
                ripple::lsfMPTCanEscrow,
            kISSUANCE1_OUTSTANDING_AMOUNT,
            std::nullopt,
            kISSUANCE1_ASSET_SCALE,
            kISSUANCE1_MAX_AMOUNT
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kACCOUNT,
            2,
            kISSUANCE2_METADATA,
            ripple::lsfMPTLocked | ripple::lsfMPTCanLock | ripple::lsfMPTCanClawback,
            kISSUANCE2_OUTSTANDING_AMOUNT,
            kISSUANCE2_TRANSFER_FEE,
            std::nullopt,
            kISSUANCE2_MAX_AMOUNT,
            kISSUANCE2_LOCKED_AMOUNT,
            kISSUANCE2_DOMAIN_ID_HEX
        )
            .getSerializer()
            .peekData()
    };

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                AccountMPTokenIssuancesHandler::kLIMIT_MAX + 1
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
            kACCOUNT,
            kLEDGER_HASH,
            AccountMPTokenIssuancesHandler::kLIMIT_MAX,
            kISSUANCE_OUT1,
            kISSUANCE_OUT2
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, EmptyResult)
{
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject({}, kISSUANCE_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
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
    uint32_t const mutableFlags1 = ripple::lsmfMPTCanMutateCanLock |
        ripple::lsmfMPTCanMutateRequireAuth | ripple::lsmfMPTCanMutateCanEscrow |
        ripple::lsmfMPTCanMutateCanTrade;

    uint32_t const mutableFlags2 = ripple::lsmfMPTCanMutateCanTransfer |
        ripple::lsmfMPTCanMutateCanClawback | ripple::lsmfMPTCanMutateMetadata |
        ripple::lsmfMPTCanMutateTransferFee;

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{
        createMptIssuanceObject(
            kACCOUNT,
            3,
            std::nullopt,
            ripple::lsfMPTCanTransfer,
            kISSUANCE1_OUTSTANDING_AMOUNT,
            kISSUANCE1_TRANSFER_FEE,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            mutableFlags1
        )
            .getSerializer()
            .peekData(),

        createMptIssuanceObject(
            kACCOUNT,
            5,
            kISSUANCE2_METADATA,
            ripple::lsfMPTCanTransfer,
            kISSUANCE2_OUTSTANDING_AMOUNT,
            kISSUANCE2_TRANSFER_FEE,
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
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
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
            kACCOUNT,
            kLEDGER_HASH,
            kISSUANCE_INDEX1,
            kACCOUNT,
            kISSUANCE1_OUTSTANDING_AMOUNT,
            kISSUANCE1_TRANSFER_FEE,
            kISSUANCE_INDEX2,
            kACCOUNT,
            kISSUANCE2_OUTSTANDING_AMOUNT,
            kISSUANCE2_TRANSFER_FEE,
            kISSUANCE2_METADATA_HEX
        );

        auto const handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
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
        {.testName = "Locked", .flag = ripple::lsfMPTLocked, .expectedJsonKey = "mpt_locked"},
        {.testName = "CanLock", .flag = ripple::lsfMPTCanLock, .expectedJsonKey = "mpt_can_lock"},
        {.testName = "RequireAuth",
         .flag = ripple::lsfMPTRequireAuth,
         .expectedJsonKey = "mpt_require_auth"},
        {.testName = "CanEscrow",
         .flag = ripple::lsfMPTCanEscrow,
         .expectedJsonKey = "mpt_can_escrow"},
        {.testName = "CanTrade",
         .flag = ripple::lsfMPTCanTrade,
         .expectedJsonKey = "mpt_can_trade"},
        {.testName = "CanTransfer",
         .flag = ripple::lsfMPTCanTransfer,
         .expectedJsonKey = "mpt_can_transfer"},
        {.testName = "CanClawback",
         .flag = ripple::lsfMPTCanClawback,
         .expectedJsonKey = "mpt_can_clawback"},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesImmutableFlagsGroup,
    AccountMPTokenIssuancesImmutableFlagsTest,
    ValuesIn(generateSingleFlagTests()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountMPTokenIssuancesImmutableFlagsTest, SingleFlag)
{
    auto const testParams = GetParam();

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kISSUANCE_INDEX1}}, kISSUANCE_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs =
        std::vector<Blob>{createMptIssuanceObject(kACCOUNT, 1, std::nullopt, testParams.flag, 0)
                              .getSerializer()
                              .peekData()};

    EXPECT_CALL(*backend_, doFetchLedgerObjects).WillOnce(Return(bbs));

    runSpawn([this, &testParams](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
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
         .mutableFlag = ripple::lsmfMPTCanMutateCanLock,
         .expectedJsonKey = "mpt_can_mutate_can_lock"},
        {.testName = "CanMutateRequireAuth",
         .mutableFlag = ripple::lsmfMPTCanMutateRequireAuth,
         .expectedJsonKey = "mpt_can_mutate_require_auth"},
        {.testName = "CanMutateCanEscrow",
         .mutableFlag = ripple::lsmfMPTCanMutateCanEscrow,
         .expectedJsonKey = "mpt_can_mutate_can_escrow"},
        {.testName = "CanMutateCanTrade",
         .mutableFlag = ripple::lsmfMPTCanMutateCanTrade,
         .expectedJsonKey = "mpt_can_mutate_can_trade"},
        {.testName = "CanMutateCanTransfer",
         .mutableFlag = ripple::lsmfMPTCanMutateCanTransfer,
         .expectedJsonKey = "mpt_can_mutate_can_transfer"},
        {.testName = "CanMutateCanClawback",
         .mutableFlag = ripple::lsmfMPTCanMutateCanClawback,
         .expectedJsonKey = "mpt_can_mutate_can_clawback"},
        {.testName = "CanMutateMetadata",
         .mutableFlag = ripple::lsmfMPTCanMutateMetadata,
         .expectedJsonKey = "mpt_can_mutate_metadata"},
        {.testName = "CanMutateTransferFee",
         .mutableFlag = ripple::lsmfMPTCanMutateTransferFee,
         .expectedJsonKey = "mpt_can_mutate_transfer_fee"},
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCAccountMPTokenIssuancesMutableFlagsGroup,
    AccountMPTokenIssuancesMutableFlagsTest,
    ValuesIn(generateSingleMutableFlagTests()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountMPTokenIssuancesMutableFlagsTest, SingleMutableFlag)
{
    auto const testParams = GetParam();

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKk, _, _))
        .WillOnce(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kISSUANCE_INDEX1}}, kISSUANCE_INDEX1);
    EXPECT_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _))
        .WillOnce(Return(ownerDir.getSerializer().peekData()));

    auto const bbs = std::vector<Blob>{createMptIssuanceObject(
                                           kACCOUNT,
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
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
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
