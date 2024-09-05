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
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/impl/spawn.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using namespace rpc;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto INDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto TXNID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

class RPCHelpersTest : public util::prometheus::WithPrometheus, public MockBackendTest, public SyncAsioContextTest {
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
    }
    void
    TearDown() override
    {
        SyncAsioContextTest::TearDown();
    }
};

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidIndexNotHex)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto account = GetAccountIDWithString(ACCOUNT);
        auto ret = traverseOwnedNodes(*backend, account, 9, 10, "nothex,10", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidPageNotInt)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto account = GetAccountIDWithString(ACCOUNT);
        auto ret = traverseOwnedNodes(*backend, account, 9, 10, "nothex,abc", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx.run();
}

// limit = 10, return 2 objects
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarker)
{
    auto account = GetAccountIDWithString(ACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    // return owner index
    ripple::STObject const ownerDir =
        CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    ON_CALL(*backend, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [this, &account](boost::asio::yield_context yield) {
        auto ret = traverseOwnedNodes(*backend, account, 9, 10, {}, yield, [](auto) {

        });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(
            cursor->toString(),
            "0000000000000000000000000000000000000000000000000000000000000000,"
            "0"
        );
    });
    ctx.run();
}

// limit = 10, return 10 objects and marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnSamePageMarker)
{
    auto account = GetAccountIDWithString(ACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    std::vector<Blob> bbs;

    int objectsCount = 11;
    ripple::STObject const channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(INDEX1);
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [this, &account](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend, account, 9, 10, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, 10);
        EXPECT_EQ(cursor->toString(), fmt::format("{},0", INDEX1));
    });
    ctx.run();
}

// 10 objects per page, limit is 15, return the second page as marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnOtherPageMarker)
{
    auto account = GetAccountIDWithString(ACCOUNT);
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    constexpr static auto nextPage = 99;
    constexpr static auto limit = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), nextPage).key;

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(INDEX1);
        objectsCount--;
    }
    objectsCount = 15;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, nextPage);
    // first page 's next page is 99
    ON_CALL(*backend, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = CreateOwnerDirLedgerObject(indexes, INDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend, account, 9, limit, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, limit);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", INDEX1, nextPage));
    });
    ctx.run();
}

// Send a valid marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithMarkerReturnSamePageMarker)
{
    auto account = GetAccountIDWithString(ACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    constexpr static auto limit = 8;
    constexpr static auto pageNum = 99;
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(INDEX1);
        objectsCount--;
    }
    objectsCount = 10;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend, account, 9, limit, fmt::format("{},{}", INDEX1, pageNum), yield, [&](auto) { count++; }
        );
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, limit);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", INDEX1, pageNum));
    });
    ctx.run();
}

// Send a valid marker, but marker contain an unexisting index
// return invalid params error
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithUnexistingIndexMarker)
{
    auto account = GetAccountIDWithString(ACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    constexpr static auto limit = 8;
    constexpr static auto pageNum = 99;
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    int objectsCount = 10;
    ripple::STObject const channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(INDEX1);
        objectsCount--;
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend, account, 9, limit, fmt::format("{},{}", INDEX2, pageNum), yield, [&](auto) { count++; }
        );
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Invalid marker.");
    });
    ctx.run();
}

TEST_F(RPCHelpersTest, EncodeCTID)
{
    auto const ctid = encodeCTID(0x1234, 0x67, 0x89);
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, "C000123400670089");
    EXPECT_FALSE(encodeCTID(0x1FFFFFFF, 0x67, 0x89));
}

TEST_F(RPCHelpersTest, DecodeCTIDString)
{
    auto const ctid = decodeCTID("C000123400670089");
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, std::make_tuple(0x1234, 0x67, 0x89));
    EXPECT_FALSE(decodeCTID("F000123400670089"));
    EXPECT_FALSE(decodeCTID("F0001234006700"));
    EXPECT_FALSE(decodeCTID("F000123400*700"));
}

TEST_F(RPCHelpersTest, DecodeCTIDInt)
{
    uint64_t ctidStr = 0xC000123400670089;
    auto const ctid = decodeCTID(ctidStr);
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, std::make_tuple(0x1234, 0x67, 0x89));
    ctidStr = 0xF000123400670089;
    EXPECT_FALSE(decodeCTID(ctidStr));
}

TEST_F(RPCHelpersTest, DecodeInvalidCTID)
{
    EXPECT_FALSE(decodeCTID('c'));
    EXPECT_FALSE(decodeCTID(true));
}

TEST_F(RPCHelpersTest, DeliverMaxAliasV1)
{
    std::array<std::string, 3> const inputArray = {
        R"({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })"
    };

    std::array<std::string, 3> outputArray = {
        R"({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            },
            "DeliverMax": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })"
    };

    for (size_t i = 0; i < inputArray.size(); i++) {
        auto req = boost::json::parse(inputArray[i]).as_object();
        insertDeliverMaxAlias(req, 1);
        EXPECT_EQ(req, boost::json::parse(outputArray[i]).as_object());
    }
}

TEST_F(RPCHelpersTest, DeliverMaxAliasV2)
{
    auto req = boost::json::parse(
                   R"({
                        "TransactionType": "Payment",
                        "Amount": {
                            "test": "test"
                        }
                    })"
    )
                   .as_object();

    insertDeliverMaxAlias(req, 2);
    EXPECT_EQ(
        req,
        boost::json::parse(
            R"({
                "TransactionType": "Payment",
                "DeliverMax": {
                    "test": "test"
                }
            })"
        )
    );
}

TEST_F(RPCHelpersTest, LedgerHeaderJson)
{
    auto const ledgerHeader = CreateLedgerHeader(INDEX1, 30);
    auto const binJson = toJson(ledgerHeader, true, 1u);

    auto constexpr EXPECTBIN = R"({
                                    "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                                    "closed": true
                                })";
    EXPECT_EQ(binJson, boost::json::parse(EXPECTBIN));

    auto const EXPECTJSON = fmt::format(
        R"({{
            "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "close_flags": 0,
            "close_time": 0,
            "close_time_resolution": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "{}",
            "ledger_index": "{}",
            "parent_close_time": 0,
            "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "total_coins": "0",
            "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "closed": true
        }})",
        INDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 1u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(EXPECTJSON));
}

TEST_F(RPCHelpersTest, LedgerHeaderJsonV2)
{
    auto const ledgerHeader = CreateLedgerHeader(INDEX1, 30);

    auto const EXPECTJSON = fmt::format(
        R"({{
            "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "close_flags": 0,
            "close_time": 0,
            "close_time_resolution": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "{}",
            "ledger_index": {},
            "parent_close_time": 0,
            "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "total_coins": "0",
            "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "closed": true
        }})",
        INDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 2u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(EXPECTJSON));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV1)
{
    auto const txMeta = CreateAcceptNFTOfferTxWithMetadata(ACCOUNT, 30, 1, INDEX1);
    auto const json = toJsonWithBinaryTx(txMeta, 1);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta)));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV2)
{
    auto const txMeta = CreateAcceptNFTOfferTxWithMetadata(ACCOUNT, 30, 1, INDEX1);
    auto const json = toJsonWithBinaryTx(txMeta, 2);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta_blob)));
}

TEST_F(RPCHelpersTest, ParseIssue)
{
    auto issue = parseIssue(boost::json::parse(
                                R"({
                                        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                        "currency": "JPY"
                                    })"
    )
                                .as_object());
    EXPECT_TRUE(issue.account == GetAccountIDWithString(ACCOUNT2));

    issue = parseIssue(boost::json::parse(R"({"currency": "XRP"})").as_object());
    EXPECT_TRUE(ripple::isXRP(issue.currency));

    EXPECT_THROW(parseIssue(boost::json::parse(R"({"currency": 2})").as_object()), std::runtime_error);

    EXPECT_THROW(parseIssue(boost::json::parse(R"({"currency": "XRP2"})").as_object()), std::runtime_error);

    EXPECT_THROW(
        parseIssue(boost::json::parse(
                       R"({
                                "issuer": "abcd",
                                "currency": "JPY"
                            })"
        )
                       .as_object()),
        std::runtime_error
    );

    EXPECT_THROW(
        parseIssue(boost::json::parse(R"({"issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"})").as_object()),
        std::runtime_error
    );
}

struct IsAdminCmdParamTestCaseBundle {
    std::string testName;
    std::string method;
    std::string testJson;
    bool expected;
};

struct IsAdminCmdParameterTest : public TestWithParam<IsAdminCmdParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<IsAdminCmdParamTestCaseBundle>{
        {"featureVetoedTrue", "feature", R"({"vetoed": true, "feature": "foo"})", true},
        {"featureVetoedFalse", "feature", R"({"vetoed": false, "feature": "foo"})", true},
        {"ledgerFullTrue", "ledger", R"({"full": true})", true},
        {"ledgerAccountsTrue", "ledger", R"({"accounts": true})", true},
        {"ledgerTypeTrue", "ledger", R"({"type": true})", true},
        {"ledgerFullFalse", "ledger", R"({"full": false})", false},
        {"ledgerAccountsFalse", "ledger", R"({"accounts": false})", false},
        {"ledgerTypeFalse", "ledger", R"({"type": false})", false},
        {"ledgerEntry", "ledger_entry", R"({"type": false})", false}
    };
}

INSTANTIATE_TEST_CASE_P(
    IsAdminCmdTest,
    IsAdminCmdParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(IsAdminCmdParameterTest, Test)
{
    auto const testBundle = GetParam();
    EXPECT_EQ(isAdminCmd(testBundle.method, boost::json::parse(testBundle.testJson).as_object()), testBundle.expected);
}
