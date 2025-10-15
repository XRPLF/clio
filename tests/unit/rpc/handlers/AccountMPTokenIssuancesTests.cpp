//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "rpc/handlers/AccountMPTokenIssuances.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

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
constexpr auto kISSUANCE_INDEX1 = "A6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kISSUANCE_INDEX2 = "B6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

// unique values for issuance1
constexpr uint64_t kISSUANCE1_MAX_AMOUNT = 10000;
constexpr uint64_t kISSUANCE1_OUTSTANDING_AMOUNT = 5000;
constexpr uint8_t kISSUANCE1_ASSET_SCALE = 2;

// unique values for issuance2
constexpr uint64_t kISSUANCE2_MAX_AMOUNT = 20000;
constexpr uint64_t kISSUANCE2_OUTSTANDING_AMOUNT = 800;
constexpr uint64_t kISSUANCE2_LOCKED_AMOUNT = 100;
constexpr uint16_t kISSUANCE2_TRANSFER_FEE = 5;
constexpr auto kISSUANCE2_METADATA = "test-meta";
constexpr auto kISSUANCE2_METADATA_HEX = "746573742D6D657461";
constexpr auto kISSUANCE2_DOMAIN_ID_HEX = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

// define expected JSON for mpt issuances
auto const kISSUANCE_OUT1 = fmt::format(
    R"JSON({{
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
    kACCOUNT,
    kISSUANCE1_MAX_AMOUNT,
    kISSUANCE1_OUTSTANDING_AMOUNT,
    kISSUANCE1_ASSET_SCALE
);

auto const kISSUANCE_OUT2 = fmt::format(
    R"JSON({{
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

struct AccountMPTokenIssuancesParameterTest : RPCAccountMPTokenIssuancesHandlerTest,
                                              WithParamInterface<AccountMPTokenIssuancesParamTestCaseBundle> {};

// generate values for invalid params test
static auto
generateTestValuesForInvalidParamsTest()
{
    return std::vector<AccountMPTokenIssuancesParamTestCaseBundle>{
        {"NonHexLedgerHash",
         fmt::format(R"JSON({{ "account": "{}", "ledger_hash": "xxx" }})JSON", kACCOUNT),
         "invalidParams",
         "ledger_hashMalformed"},
        {"NonStringLedgerHash",
         fmt::format(R"JSON({{ "account": "{}", "ledger_hash": 123 }})JSON", kACCOUNT),
         "invalidParams",
         "ledger_hashNotString"},
        {"InvalidLedgerIndexString",
         fmt::format(R"JSON({{ "account": "{}", "ledger_index": "notvalidated" }})JSON", kACCOUNT),
         "invalidParams",
         "ledgerIndexMalformed"},
        {"MarkerNotString",
         fmt::format(R"JSON({{ "account": "{}", "marker": 9 }})JSON", kACCOUNT),
         "invalidParams",
         "markerNotString"},
        {"InvalidMarkerContent",
         fmt::format(R"JSON({{ "account": "{}", "marker": "123invalid" }})JSON", kACCOUNT),
         "invalidParams",
         "Malformed cursor."},
        {"AccountMissing", R"JSON({ "limit": 10 })JSON", "invalidParams", "Required field 'account' missing"},
        {"AccountNotString", R"JSON({ "account": 123 })JSON", "actMalformed", "Account malformed."},
        {"AccountMalformed",
         R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp" })JSON",
         "actMalformed",
         "Account malformed."},
        {"LimitNotInteger",
         fmt::format(R"JSON({{ "account": "{}", "limit": "t" }})JSON", kACCOUNT),
         "invalidParams",
         "Invalid parameters."},
        {"LimitNegative",
         fmt::format(R"JSON({{ "account": "{}", "limit": -1 }})JSON", kACCOUNT),
         "invalidParams",
         "Invalid parameters."},
        {"LimitZero",
         fmt::format(R"JSON({{ "account": "{}", "limit": 0 }})JSON", kACCOUNT),
         "invalidParams",
         "Invalid parameters."},
        {"LimitTypeInvalid",
         fmt::format(R"JSON({{ "account": "{}", "limit": true }})JSON", kACCOUNT),
         "invalidParams",
         "Invalid parameters."}
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

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
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerHeader>{}));

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
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 31);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));
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
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));
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
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // return non-empty account
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    // return two mptoken issuance objects
    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // mocking mptoken issuance ledger objects
    std::vector<Blob> bbs;
    auto const issuance1 = createMptIssuanceObject(
        kACCOUNT,
        1,
        std::nullopt,
        ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer | ripple::lsfMPTCanEscrow,
        kISSUANCE1_OUTSTANDING_AMOUNT,
        std::nullopt,
        kISSUANCE1_ASSET_SCALE,
        kISSUANCE1_MAX_AMOUNT
    );

    auto const issuance2 = createMptIssuanceObject(
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
    );

    bbs.push_back(issuance1.getSerializer().peekData());
    bbs.push_back(issuance2.getSerializer().peekData());
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
        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};

        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(expected), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, UseLimit)
{
    constexpr int limit = 20;
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;

    for (int i = 0; i < 50; ++i) {
        indexes.emplace_back(ripple::uint256{kISSUANCE_INDEX1});
        auto const issuance = createMptIssuanceObject(kACCOUNT, i);
        bbs.push_back(issuance.getSerializer().peekData());
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kISSUANCE_INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this, limit](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}",
                    "limit": {}
                }})JSON",
                kACCOUNT,
                limit
            )
        );

        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const resultJson = (*output.result).as_object();
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), limit);
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
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountMPTokenIssuancesHandler::kLIMIT_MIN);
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
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountMPTokenIssuancesHandler::kLIMIT_MAX);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, MarkerOutput)
{
    constexpr auto kNEXT_PAGE = 99;
    constexpr auto kLIMIT = 15;
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<ripple::uint256> indexes;
    for (int i = 0; i < 10; ++i) {
        indexes.emplace_back(kISSUANCE_INDEX1);
    }

    std::vector<Blob> bbs;
    for (int i = 0; i < kLIMIT; ++i) {
        bbs.push_back(createMptIssuanceObject(kACCOUNT, i).getSerializer().peekData());
    }
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
        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        auto const& resultJson = (*output.result).as_object();
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

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDirKk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    std::vector<ripple::uint256> indexes;
    for (int i = 0; i < kLIMIT; ++i) {
        indexes.emplace_back(kISSUANCE_INDEX1);
        bbs.push_back(createMptIssuanceObject(kACCOUNT, i).getSerializer().peekData());
    }

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
        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const& resultJson = (*output.result).as_object();
        EXPECT_TRUE(resultJson.if_contains("marker") == nullptr);
        EXPECT_EQ(resultJson.at("mpt_issuances").as_array().size(), kLIMIT - 1);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitLessThanMin)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto const issuance1 = createMptIssuanceObject(
        kACCOUNT,
        1,
        std::nullopt,
        ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer | ripple::lsfMPTCanEscrow,
        kISSUANCE1_OUTSTANDING_AMOUNT,
        std::nullopt,
        kISSUANCE1_ASSET_SCALE,
        kISSUANCE1_MAX_AMOUNT
    );

    auto const issuance2 = createMptIssuanceObject(
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
    );

    bbs.push_back(issuance1.getSerializer().peekData());
    bbs.push_back(issuance2.getSerializer().peekData());

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

        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, LimitMoreThanMax)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(
        {ripple::uint256{kISSUANCE_INDEX1}, ripple::uint256{kISSUANCE_INDEX2}}, kISSUANCE_INDEX1
    );
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    auto const issuance1 = createMptIssuanceObject(
        kACCOUNT,
        1,
        std::nullopt,
        ripple::lsfMPTCanTrade | ripple::lsfMPTRequireAuth | ripple::lsfMPTCanTransfer | ripple::lsfMPTCanEscrow,
        kISSUANCE1_OUTSTANDING_AMOUNT,
        std::nullopt,
        kISSUANCE1_ASSET_SCALE,
        kISSUANCE1_MAX_AMOUNT
    );

    auto const issuance2 = createMptIssuanceObject(
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
    );

    bbs.push_back(issuance1.getSerializer().peekData());
    bbs.push_back(issuance2.getSerializer().peekData());

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

        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountMPTokenIssuancesHandlerTest, EmptyResult)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, _, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    ripple::STObject const ownerDir = createOwnerDirLedgerObject({}, kISSUANCE_INDEX1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, _, _)).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    runSpawn([this](auto yield) {
        auto const input = json::parse(
            fmt::format(
                R"JSON({{
                    "account": "{}"
                }})JSON",
                kACCOUNT
            )
        );
        auto handler = AnyHandler{AccountMPTokenIssuancesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("mpt_issuances").as_array().size(), 0);
    });
}
