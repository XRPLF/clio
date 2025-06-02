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

#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/TestObject.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/asio/impl/spawn.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

using namespace data;
using namespace rpc;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kINDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTXN_ID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kLEDGER_SEQ_OBJECT = 50;
constexpr auto kCURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kAMM_ACCOUNT = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr auto kISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLPTOKEN_CURRENCY = "037C35306B24AAB7FF90848206E003279AA47090";
constexpr auto kAMM_ID = 54321;

}  // namespace

class RPCHelpersTest : public util::prometheus::WithPrometheus, public MockBackendTest, public SyncAsioContextTest {
public:
    RPCHelpersTest()
    {
        backend_->setRange(10, 300);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidIndexNotHex)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kACCOUNT);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,10", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidPageNotInt)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kACCOUNT);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,abc", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx_.run();
}

// limit = 10, return 2 objects
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    // return owner index
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [](auto) {

        });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(
            cursor->toString(),
            "0000000000000000000000000000000000000000000000000000000000000000,"
            "0"
        );
    });
    ctx_.run();
}

// limit = 10, return 10 objects and marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    std::vector<Blob> bbs;

    int objectsCount = 11;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, 10);
        EXPECT_EQ(cursor->toString(), fmt::format("{},0", kINDEX1));
    });
    ctx_.run();
}

// 10 objects per page, limit is 15, return the second page as marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnOtherPageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    static constexpr auto kNEXT_PAGE = 99;
    static constexpr auto kLIMIT = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    objectsCount = 15;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    // first page 's next page is 99
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kINDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend_, account, 9, kLIMIT, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, kLIMIT);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", kINDEX1, kNEXT_PAGE));
    });
    ctx_.run();
}

// Send a valid marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    static constexpr auto kLIMIT = 8;
    static constexpr auto kPAGE_NUM = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    objectsCount = 10;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_, account, 9, kLIMIT, fmt::format("{},{}", kINDEX1, kPAGE_NUM), yield, [&](auto) { count++; }
        );
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, kLIMIT);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", kINDEX1, kPAGE_NUM));
    });
    ctx_.run();
}

// Send a valid marker, but marker contain an unexisting index
// return invalid params error
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithUnexistingIndexMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    static constexpr auto kLIMIT = 8;
    static constexpr auto kPAGE_NUM = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_, account, 9, kLIMIT, fmt::format("{},{}", kINDEX2, kPAGE_NUM), yield, [&](auto) { count++; }
        );
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Invalid marker.");
    });
    ctx_.run();
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
        R"JSON({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            }
        })JSON",
        R"JSON({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })JSON",
        R"JSON({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })JSON"
    };

    std::array<std::string, 3> outputArray = {
        R"JSON({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            },
            "DeliverMax": {
                "test": "test"
            }
        })JSON",
        R"JSON({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })JSON",
        R"JSON({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })JSON"
    };

    for (size_t i = 0; i < inputArray.size(); i++) {
        auto req = boost::json::parse(inputArray[i]).as_object();
        insertDeliverMaxAlias(req, 1);
        auto const expectedReq = boost::json::parse(outputArray[i]).as_object();
        EXPECT_EQ(req, expectedReq) << req << "\n" << expectedReq;
    }
}

TEST_F(RPCHelpersTest, DeliverMaxAliasV2)
{
    auto req = boost::json::parse(
                   R"JSON({
                        "TransactionType": "Payment",
                        "Amount": {
                            "test": "test"
                        }
                    })JSON"
    )
                   .as_object();

    insertDeliverMaxAlias(req, 2);
    EXPECT_EQ(
        req,
        boost::json::parse(
            R"JSON({
                "TransactionType": "Payment",
                "DeliverMax": {
                    "test": "test"
                }
            })JSON"
        )
    );
}

TEST_F(RPCHelpersTest, LedgerHeaderJson)
{
    auto const ledgerHeader = createLedgerHeader(kINDEX1, 30);
    auto const binJson = toJson(ledgerHeader, true, 1u);

    constexpr auto kEXPECT_BIN = R"JSON({
                                    "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                                    "closed": true
                                })JSON";
    EXPECT_EQ(binJson, boost::json::parse(kEXPECT_BIN));

    auto const expectJson = fmt::format(
        R"JSON({{
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
        }})JSON",
        kINDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 1u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, LedgerHeaderJsonV2)
{
    auto const ledgerHeader = createLedgerHeader(kINDEX1, 30);

    auto const expectJson = fmt::format(
        R"JSON({{
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
        }})JSON",
        kINDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 2u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV1)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kACCOUNT, 30, 1, kINDEX1, kINDEX2);
    auto const json = toJsonWithBinaryTx(txMeta, 1);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta)));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV2)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kACCOUNT, 30, 1, kINDEX1, kINDEX2);
    auto const json = toJsonWithBinaryTx(txMeta, 2);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta_blob)));
}

TEST_F(RPCHelpersTest, ParseIssue)
{
    auto issue = parseIssue(boost::json::parse(
                                R"JSON({
                                        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                        "currency": "JPY"
                                    })JSON"
    )
                                .as_object());
    EXPECT_TRUE(issue.account == getAccountIdWithString(kACCOUNT2));

    issue = parseIssue(boost::json::parse(R"JSON({"currency": "XRP"})JSON").as_object());
    EXPECT_TRUE(ripple::isXRP(issue.currency));

    EXPECT_THROW(parseIssue(boost::json::parse(R"JSON({"currency": 2})JSON").as_object()), std::runtime_error);

    EXPECT_THROW(parseIssue(boost::json::parse(R"JSON({"currency": "XRP2"})JSON").as_object()), std::runtime_error);

    EXPECT_THROW(
        parseIssue(boost::json::parse(
                       R"JSON({
                                "issuer": "abcd",
                                "currency": "JPY"
                            })JSON"
        )
                       .as_object()),
        std::runtime_error
    );

    EXPECT_THROW(
        parseIssue(boost::json::parse(R"JSON({"issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"})JSON").as_object()),
        std::runtime_error
    );
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_BlobDoesNotExist)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuerKey = ripple::keylet::account(account);

    // returns empty blob
    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    runSpawn([&](boost::asio::yield_context yield) {
        // return false: blob doesn't exist
        EXPECT_FALSE(
            fetchAndCheckAnyFlagsExists(*backend_, kLEDGER_SEQ_OBJECT, issuerKey, {ripple::lsfHighDeepFreeze}, yield)
        );
    });
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_AccountWithCorrectFlag)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuerKey = ripple::keylet::account(account);

    // create account with highDeepFreeze Flag
    auto const accountObject = createAccountRootObject(kACCOUNT, ripple::lsfHighDeepFreeze, 1, 10, 2, kTXN_ID, 3);

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // returns true: accountObject has the highDeepFreeze flag
        EXPECT_TRUE(
            fetchAndCheckAnyFlagsExists(*backend_, kLEDGER_SEQ_OBJECT, issuerKey, {ripple::lsfHighDeepFreeze}, yield)
        );
    });
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_TrustLineIsFrozenAndCheckFreezeFlag)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuerKey = ripple::keylet::account(account);

    // create account with lowDeepFreeze Flag
    auto const accountObject = createAccountRootObject(kACCOUNT, ripple::lsfLowDeepFreeze, 1, 10, 2, kTXN_ID, 3);

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // returns false: accountObject has the lowDeepFreeze flag
        EXPECT_FALSE(
            fetchAndCheckAnyFlagsExists(*backend_, kLEDGER_SEQ_OBJECT, issuerKey, {ripple::lsfHighDeepFreeze}, yield)
        );
    });
}

TEST_F(RPCHelpersTest, isGlobalFrozen_AccountIsGlobalFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuerKey = ripple::keylet::account(account);

    auto const accountObject = createAccountRootObject(kACCOUNT, ripple::lsfGlobalFreeze, 1, 10, 2, kTXN_ID, 3);

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // returns false: accountObject has the lowDeepFreeze flag
        EXPECT_TRUE(isGlobalFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, yield));
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_TrustLineIsDeepFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const account2 = getAccountIdWithString(kACCOUNT2);

    // create a trustline between account and account2 and is deep frozen
    auto const trustLineKey = ripple::keylet::line(account, account2, ripple::Currency{kCURRENCY}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfLowDeepFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(isDeepFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, ripple::Currency{kCURRENCY}, account2, yield));
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_TrustLineIsNotDeepFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const account2 = getAccountIdWithString(kACCOUNT2);

    // create a trustline between account and account2 that is frozen (NOT DeepFrozen)
    auto const trustLineKey = ripple::keylet::line(account, account2, ripple::Currency{kCURRENCY}).key;
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfLowFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isDeepFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, ripple::Currency{kCURRENCY}, account2, yield)
        );
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_IssuerAndAccountIsSameWillNotBeDeepFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);

    auto const trustLineKey = ripple::keylet::line(account, issuer, ripple::Currency{kCURRENCY}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfLowDeepFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // both accounts are same so trustline is not deep frozen
        EXPECT_FALSE(isDeepFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, ripple::Currency{kCURRENCY}, account, yield));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerAccountIsGlobalFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);

    auto const accountObject = createAccountRootObject(kACCOUNT2, ripple::lsfGlobalFreeze, 1, 10, 2, kTXN_ID, 3);
    auto const issuerKey = ripple::keylet::account(issuer).key;

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(isFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, ripple::Currency{kCURRENCY}, issuer, yield));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerAndAccountIsSameWillNotBeFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);

    auto const trustLineKey = ripple::keylet::line(account, issuer, ripple::Currency{kCURRENCY}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, ripple::Currency{kCURRENCY}, account, yield));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerTrustLineIsFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);
    ripple::Currency const currency{kCURRENCY};

    auto const trustLineKey = ripple::keylet::line(account, issuer, currency).key;

    // issuer is higher than account, so the correct flag to set is High freeze
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(isFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, currency, issuer, yield));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerWithLowFreezeIsNotFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);
    ripple::Currency const currency{kCURRENCY};

    auto const trustLineKey = ripple::keylet::line(account, issuer, currency).key;

    // issuer is higher than account, but the flag set here is low freeze
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kACCOUNT, 8, kACCOUNT, 1000, kACCOUNT2, 2000, kINDEX1, 2, ripple::lsfLowFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isFrozen(*backend_, kLEDGER_SEQ_OBJECT, account, currency, issuer, yield));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_TrustLineNotfrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);
    ripple::Currency const currency{kCURRENCY};

    auto const trustLineKey = ripple::keylet::line(account, issuer, currency).key;
    auto const trustLine =
        createRippleStateLedgerObject(kCURRENCY, kACCOUNT2, 500, kACCOUNT, 1000, kACCOUNT2, 1000, kTXN_ID, 1, 0);

    EXPECT_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillOnce(Return(trustLine.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_, *mockAmendmentCenterPtr_, kLEDGER_SEQ_OBJECT, account, currency, issuer, false, yield
        );
        // Check issuer has a balance of 500
        EXPECT_EQ(result, ripple::STAmount(getIssue(kCURRENCY, kACCOUNT2), 500));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_NoTrustLine)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);
    ripple::Currency const currency{kCURRENCY};

    auto const key = ripple::keylet::line(account, issuer, currency).key;

    // return no trustline found
    EXPECT_CALL(*backend_, doFetchLedgerObject(key, kLEDGER_SEQ_OBJECT, _)).WillOnce(Return(std::nullopt));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_, *mockAmendmentCenterPtr_, kLEDGER_SEQ_OBJECT, account, currency, issuer, false, yield
        );
        // balance is 0 as trustline is frozen
        EXPECT_EQ(result, ripple::STAmount(getIssue(kCURRENCY, kACCOUNT2), 0));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_TrustLineButFrozen)
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const issuer = getAccountIdWithString(kACCOUNT2);
    ripple::Currency const currency{kCURRENCY};

    // balance of 500, but trustline is frozen
    auto const trustLineKey = ripple::keylet::line(account, issuer, currency).key;

    auto const trustLine = createRippleStateLedgerObject(
        kCURRENCY, kACCOUNT2, 500, kACCOUNT, 1000, kACCOUNT2, 1000, kTXN_ID, 1, ripple::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLEDGER_SEQ_OBJECT, _))
        .WillByDefault(Return(trustLine.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_, *mockAmendmentCenterPtr_, kLEDGER_SEQ_OBJECT, account, currency, issuer, true, yield
        );
        EXPECT_EQ(result, ripple::STAmount(getIssue(kCURRENCY, kACCOUNT2), 0));
    });
}

TEST_F(RPCHelpersTest, AccountHoldsFixLPTAmendmentDisabled)
{
    auto ammAccount = getAccountIdWithString(kAMM_ACCOUNT);
    auto account = getAccountIdWithString(kACCOUNT);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLPTOKEN_CURRENCY, kAMM_ACCOUNT, 100, kACCOUNT, 100, kAMM_ACCOUNT, 100, kTXN_ID, 3
    );
    auto const lptRippleStateKk = ripple::keylet::line(ammAccount, account, ripple::to_currency(kLPTOKEN_CURRENCY)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    auto const ammID = ripple::uint256{kAMM_ID};
    auto const ammAccountKk = ripple::keylet::account(ammAccount).key;
    auto const ammAccountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2, 0, ammID);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .WillOnce(Return(ammAccountRoot.getSerializer().peekData()));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_))
        .WillOnce(Return(false));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            ripple::to_currency(kLPTOKEN_CURRENCY),
            ammAccount,
            true,
            yield
        );
        EXPECT_EQ(ret.mantissa(), 1000000000000000);
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, AccountHoldsLPTokenNotAMMAccount)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto account2 = getAccountIdWithString(kACCOUNT2);

    auto const usdRippleState =
        createRippleStateLedgerObject("USD", kACCOUNT2, 100, kACCOUNT, 100, kACCOUNT2, 100, kTXN_ID, 3);
    auto const usdRippleStateKk = ripple::keylet::line(account2, account, ripple::to_currency("USD")).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(usdRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(usdRippleState.getSerializer().peekData()));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_))
        .WillOnce(Return(true));

    auto const account2Kk = ripple::keylet::account(account2).key;
    auto const account2Root = createAccountRootObject(kACCOUNT2, 0, 2, 200, 2, kINDEX1, 2, 0);

    EXPECT_CALL(*backend_, doFetchLedgerObject(account2Kk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(account2Root.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_, *mockAmendmentCenterPtr_, 0, account, ripple::to_currency("USD"), account2, true, yield
        );
        EXPECT_EQ(ret.mantissa(), 1000000000000000);
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, AccountHoldsLPTokenAsset1Frozen)
{
    auto ammAccount = getAccountIdWithString(kAMM_ACCOUNT);
    auto account = getAccountIdWithString(kACCOUNT);
    auto issuer = getAccountIdWithString(kISSUER);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLPTOKEN_CURRENCY, kAMM_ACCOUNT, 100, kACCOUNT, 100, kAMM_ACCOUNT, 100, kTXN_ID, 3
    );
    auto const lptRippleStateKk = ripple::keylet::line(ammAccount, account, ripple::to_currency(kLPTOKEN_CURRENCY)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_))
        .WillOnce(Return(true));

    auto const ammID = ripple::uint256{kAMM_ID};
    auto const ammAccountKk = ripple::keylet::account(ammAccount).key;
    auto const ammAccountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm = createAmmObject(kAMM_ACCOUNT, "USD", kISSUER, "XRP", ripple::toBase58(ripple::xrpAccount()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::amm(ammID).key, testing::_, testing::_))
        .Times(1)
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = ripple::keylet::account(issuer).key;
    auto const issuerAccountRoot = createAccountRootObject(kISSUER, ripple::lsfGlobalFreeze, 2, 200, 2, kINDEX1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            ripple::to_currency(kLPTOKEN_CURRENCY),
            ammAccount,
            true,
            yield
        );
        EXPECT_EQ(ret.mantissa(), 0);
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, AccountHoldsLPTokenAsset2Frozen)
{
    auto ammAccount = getAccountIdWithString(kAMM_ACCOUNT);
    auto account = getAccountIdWithString(kACCOUNT);
    auto issuer = getAccountIdWithString(kISSUER);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLPTOKEN_CURRENCY, kAMM_ACCOUNT, 100, kACCOUNT, 100, kAMM_ACCOUNT, 100, kTXN_ID, 3
    );
    auto const lptRippleStateKk = ripple::keylet::line(ammAccount, account, ripple::to_currency(kLPTOKEN_CURRENCY)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_)).Times(2);
    ON_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .WillByDefault(testing::Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_))
        .WillOnce(testing::Return(true));

    auto const ammID = ripple::uint256{kAMM_ID};
    auto const ammAccountKk = ripple::keylet::account(ammAccount).key;
    auto const ammAccountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm = createAmmObject(kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "USD", kISSUER);
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::amm(ammID).key, testing::_, testing::_))
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = ripple::keylet::account(issuer).key;
    auto const issuerAccountRoot = createAccountRootObject(kISSUER, ripple::lsfGlobalFreeze, 2, 200, 2, kINDEX1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            ripple::to_currency(kLPTOKEN_CURRENCY),
            ammAccount,
            true,
            yield
        );
        EXPECT_EQ(ret.mantissa(), 0);
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, AccountHoldsLPTokenUnfrozen)
{
    auto ammAccount = getAccountIdWithString(kAMM_ACCOUNT);
    auto account = getAccountIdWithString(kACCOUNT);
    auto issuer = getAccountIdWithString(kISSUER);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLPTOKEN_CURRENCY, kAMM_ACCOUNT, 100, kACCOUNT, 100, kAMM_ACCOUNT, 100, kTXN_ID, 3
    );
    auto const lptRippleStateKk = ripple::keylet::line(ammAccount, account, ripple::to_currency(kLPTOKEN_CURRENCY)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_))
        .WillOnce(Return(true));

    auto const ammID = ripple::uint256{kAMM_ID};
    auto const ammAccountKk = ripple::keylet::account(ammAccount).key;
    auto const ammAccountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm = createAmmObject(kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "USD", kISSUER);
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::amm(ammID).key, testing::_, testing::_))
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = ripple::keylet::account(issuer).key;
    auto const issuerAccountRoot = createAccountRootObject(kISSUER, 0, 2, 200, 2, kINDEX1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    auto const usdRippleState =
        createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 100, kISSUER, 100, kTXN_ID, 3);
    auto const usdRippleStateKk = ripple::keylet::line(issuer, account, ripple::to_currency("USD")).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject(usdRippleStateKk, testing::_, testing::_))
        .WillOnce(Return(usdRippleState.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            ripple::to_currency(kLPTOKEN_CURRENCY),
            ammAccount,
            true,
            yield
        );
        EXPECT_EQ(ret.mantissa(), 1000000000000000);
    });
    ctx_.run();
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
        {.testName = "ledgerEntry",
         .method = "ledger_entry",
         .testJson = R"JSON({"type": false})JSON",
         .expected = false},

        {.testName = "featureVetoedTrue",
         .method = "feature",
         .testJson = R"JSON({"vetoed": true, "feature": "foo"})JSON",
         .expected = true},
        {.testName = "featureVetoedFalse",
         .method = "feature",
         .testJson = R"JSON({"vetoed": false, "feature": "foo"})JSON",
         .expected = true},
        {.testName = "featureVetoedIsStr",
         .method = "feature",
         .testJson = R"JSON({"vetoed": "String"})JSON",
         .expected = true},

        {.testName = "ledger", .method = "ledger", .testJson = R"JSON({})JSON", .expected = false},
        {.testName = "ledgerWithType", .method = "ledger", .testJson = R"JSON({"type": "fee"})JSON", .expected = false},
        {.testName = "ledgerFullTrue", .method = "ledger", .testJson = R"JSON({"full": true})JSON", .expected = true},
        {.testName = "ledgerFullFalse", .method = "ledger", .testJson = R"JSON({"full": false})JSON", .expected = false
        },
        {.testName = "ledgerFullIsStr",
         .method = "ledger",
         .testJson = R"JSON({"full": "String"})JSON",
         .expected = true},
        {.testName = "ledgerFullIsEmptyStr",
         .method = "ledger",
         .testJson = R"JSON({"full": ""})JSON",
         .expected = false},
        {.testName = "ledgerFullIsNumber1", .method = "ledger", .testJson = R"JSON({"full": 1})JSON", .expected = true},
        {.testName = "ledgerFullIsNumber0", .method = "ledger", .testJson = R"JSON({"full": 0})JSON", .expected = false
        },
        {.testName = "ledgerFullIsNull", .method = "ledger", .testJson = R"JSON({"full": null})JSON", .expected = false
        },
        {.testName = "ledgerFullIsFloat0", .method = "ledger", .testJson = R"JSON({"full": 0.0})JSON", .expected = false
        },
        {.testName = "ledgerFullIsFloat1", .method = "ledger", .testJson = R"JSON({"full": 0.1})JSON", .expected = true
        },
        {.testName = "ledgerFullIsArray", .method = "ledger", .testJson = R"JSON({"full": [1]})JSON", .expected = true},
        {.testName = "ledgerFullIsEmptyArray",
         .method = "ledger",
         .testJson = R"JSON({"full": []})JSON",
         .expected = false},
        {.testName = "ledgerFullIsObject",
         .method = "ledger",
         .testJson = R"JSON({"full": {"key": 1}})JSON",
         .expected = true},
        {.testName = "ledgerFullIsEmptyObject",
         .method = "ledger",
         .testJson = R"JSON({"full": {}})JSON",
         .expected = false},

        {.testName = "ledgerAccountsTrue",
         .method = "ledger",
         .testJson = R"JSON({"accounts": true})JSON",
         .expected = true},
        {.testName = "ledgerAccountsFalse",
         .method = "ledger",
         .testJson = R"JSON({"accounts": false})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsStr",
         .method = "ledger",
         .testJson = R"JSON({"accounts": "String"})JSON",
         .expected = true},
        {.testName = "ledgerAccountsIsEmptyStr",
         .method = "ledger",
         .testJson = R"JSON({"accounts": ""})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsNumber1",
         .method = "ledger",
         .testJson = R"JSON({"accounts": 1})JSON",
         .expected = true},
        {.testName = "ledgerAccountsIsNumber0",
         .method = "ledger",
         .testJson = R"JSON({"accounts": 0})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsNull",
         .method = "ledger",
         .testJson = R"JSON({"accounts": null})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsFloat0",
         .method = "ledger",
         .testJson = R"JSON({"accounts": 0.0})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsFloat1",
         .method = "ledger",
         .testJson = R"JSON({"accounts": 0.1})JSON",
         .expected = true},
        {.testName = "ledgerAccountsIsArray",
         .method = "ledger",
         .testJson = R"JSON({"accounts": [1]})JSON",
         .expected = true},
        {.testName = "ledgerAccountsIsEmptyArray",
         .method = "ledger",
         .testJson = R"JSON({"accounts": []})JSON",
         .expected = false},
        {.testName = "ledgerAccountsIsObject",
         .method = "ledger",
         .testJson = R"JSON({"accounts": {"key": 1}})JSON",
         .expected = true},
        {.testName = "ledgerAccountsIsEmptyObject",
         .method = "ledger",
         .testJson = R"JSON({"accounts": {}})JSON",
         .expected = false},
    };
}

INSTANTIATE_TEST_CASE_P(
    IsAdminCmdTest,
    IsAdminCmdParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(IsAdminCmdParameterTest, Test)
{
    auto const testBundle = GetParam();
    EXPECT_EQ(isAdminCmd(testBundle.method, boost::json::parse(testBundle.testJson).as_object()), testBundle.expected);
}

struct RPCHelpersLogDurationTestBundle {
    std::string testName;
    std::chrono::milliseconds duration;
    std::string expectedLogLevel;
    bool expectDuration;
};

struct RPCHelpersLogDurationTest : LoggerFixture, testing::WithParamInterface<RPCHelpersLogDurationTestBundle> {
    boost::json::object const request = {
        {"method", "account_info"},
        {"params",
         boost::json::array{
             boost::json::object{{"account", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"}, {"secret", "should be deleted"}}
         }}
    };
    util::TagDecoratorFactory tagFactory{util::config::ClioConfigDefinition{
        {"log_tag_style", util::config::ConfigValue{util::config::ConfigType::String}.defaultValue("none")}
    }};
    struct DummyTaggable : util::Taggable {
        DummyTaggable(util::TagDecoratorFactory& f) : util::Taggable(f)
        {
        }
    };
    DummyTaggable taggable{tagFactory};
};

TEST_P(RPCHelpersLogDurationTest, LogDuration)
{
    auto const& tag = taggable.tag();

    logDuration(request, tag, GetParam().duration);

    std::string const output = getLoggerString();

    EXPECT_NE(output.find(GetParam().expectedLogLevel), std::string::npos) << output;
    EXPECT_NE(output.find(tag.toString()), std::string::npos);

    if (GetParam().expectDuration) {
        std::string const durationStr = std::to_string(GetParam().duration.count()) + " milliseconds";
        EXPECT_NE(output.find(durationStr), std::string::npos);
    }

    EXPECT_NE(output.find("account_info"), std::string::npos);
    EXPECT_EQ(output.find("should be deleted"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    RPCHelpersLogDurationTests,
    RPCHelpersLogDurationTest,
    testing::Values(
        RPCHelpersLogDurationTestBundle{"ShortDurationLogsAsInfo", std::chrono::milliseconds(500), "RPC:NFO", true},
        RPCHelpersLogDurationTestBundle{
            "MediumDurationLogsAsWarning",
            std::chrono::milliseconds(5000),
            "RPC:WRN",
            true
        },
        RPCHelpersLogDurationTestBundle{"LongDurationLogsAsError", std::chrono::milliseconds(15000), "RPC:ERR", true}
    ),
    tests::util::kNAME_GENERATOR
);
