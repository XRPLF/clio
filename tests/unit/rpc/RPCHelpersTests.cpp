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
#include "util/Spawn.hpp"
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
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
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
#include <vector>

using namespace data;
using namespace rpc;
using namespace testing;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kIndex1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTxnId = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kLedgerSeqObject = 50;
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kAmmAccount = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLptokenCurrency = "037C35306B24AAB7FF90848206E003279AA47090";
constexpr auto kAmmId = 54321;

}  // namespace

class RPCHelpersTest : public util::prometheus::WithPrometheus,
                       public MockBackendTest,
                       public SyncAsioContextTest {
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kAccount);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,10", yield, [](auto) {

        });
        EXPECT_FALSE(ret.has_value());
        EXPECT_EQ(ret.error(), xrpl::RpcInvalidParams);
        EXPECT_EQ(ret.error().message, "Malformed cursor.");
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidPageNotInt)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kAccount);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,abc", yield, [](auto) {

        });
        EXPECT_FALSE(ret.has_value());
        EXPECT_EQ(ret.error(), xrpl::RpcInvalidParams);
        EXPECT_EQ(ret.error().message, "Malformed cursor.");
    });
    ctx_.run();
}

// limit = 10, return 2 objects
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarker)
{
    auto account = getAccountIdWithString(kAccount);
    auto owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    // return owner index
    xrpl::STObject const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // return two payment channel objects
    std::vector<Blob> bbs;
    xrpl::STObject const channel1 =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 10, 32, kTxnId, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    util::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [](auto) {});
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(
            ret.value().toString(),
            "0000000000000000000000000000000000000000000000000000000000000000,"
            "0"
        );
    });
    ctx_.run();
}

// limit = 10, return 10 objects and marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kAccount);
    auto owneDirKk = xrpl::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    std::vector<Blob> bbs;

    int objectsCount = 11;
    xrpl::STObject const channel1 =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 10, 32, kTxnId, 28);
    std::vector<xrpl::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kIndex1);
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIndex1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    util::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [&](auto) { count++; });
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(count, 10);
        EXPECT_EQ(ret.value().toString(), fmt::format("{},0", kIndex1));
    });
    ctx_.run();
}

// 10 objects per page, limit is 15, return the second page as marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnOtherPageMarker)
{
    auto account = getAccountIdWithString(kAccount);
    auto ownerDirKk = xrpl::keylet::ownerDir(account).key;
    static constexpr auto kNextPage = 99;
    static constexpr auto kLimit = 15;
    auto ownerDir2Kk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), kNextPage).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    xrpl::STObject const channel1 =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 10, 32, kTxnId, 28);
    std::vector<xrpl::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kIndex1);
        objectsCount--;
    }
    objectsCount = 15;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIndex1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, kNextPage);
    // first page 's next page is 99
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    xrpl::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kIndex1);
    // second page's next page is 0
    ownerDir2.setFieldU64(xrpl::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret =
            traverseOwnedNodes(*backend_, account, 9, kLimit, {}, yield, [&](auto) { count++; });
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(count, kLimit);
        EXPECT_EQ(ret.value().toString(), fmt::format("{},{}", kIndex1, kNextPage));
    });
    ctx_.run();
}

// Send a valid marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kAccount);
    auto ownerDir2Kk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), 99).key;
    static constexpr auto kLimit = 8;
    static constexpr auto kPageNum = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    xrpl::STObject const channel1 =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 10, 32, kTxnId, 28);
    std::vector<xrpl::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kIndex1);
        objectsCount--;
    }
    objectsCount = 10;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIndex1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_,
            account,
            9,
            kLimit,
            fmt::format("{},{}", kIndex1, kPageNum),
            yield,
            [&](auto) { count++; }
        );
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(count, kLimit);
        EXPECT_EQ(ret.value().toString(), fmt::format("{},{}", kIndex1, kPageNum));
    });
    ctx_.run();
}

// Send a valid marker, but marker contain an unexisting index
// return invalid params error
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithUnexistingIndexMarker)
{
    auto account = getAccountIdWithString(kAccount);
    auto ownerDir2Kk = xrpl::keylet::page(xrpl::keylet::ownerDir(account), 99).key;
    static constexpr auto kLimit = 8;
    static constexpr auto kPageNum = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    int objectsCount = 10;
    xrpl::STObject const channel1 =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 10, 32, kTxnId, 28);
    std::vector<xrpl::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kIndex1);
        objectsCount--;
    }
    xrpl::STObject ownerDir = createOwnerDirLedgerObject(indexes, kIndex1);
    ownerDir.setFieldU64(xrpl::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_,
            account,
            9,
            kLimit,
            fmt::format("{},{}", kIndex2, kPageNum),
            yield,
            [&](auto) { count++; }
        );
        EXPECT_FALSE(ret.has_value());
        EXPECT_EQ(ret.error(), xrpl::RpcInvalidParams);
        EXPECT_EQ(ret.error().message, "Invalid marker.");
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, EncodeCTID)
{
    auto const ctid = encodeCTID(0x1234, 0x67, 0x89);
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, "C000123400670089");  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_FALSE(encodeCTID(0x1FFFFFFF, 0x67, 0x89));
}

TEST_F(RPCHelpersTest, DecodeCTIDString)
{
    auto const ctid = decodeCTID("C000123400670089");
    ASSERT_TRUE(ctid);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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
    constexpr auto kJson = R"JSON({
        "TransactionType": "Payment",
        "Amount": {
            "test": "test"
        }
    })JSON";
    auto req = boost::json::parse(kJson).as_object();

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
    auto const ledgerHeader = createLedgerHeader(kIndex1, 30);
    auto const binJson = toJson(ledgerHeader, true, 1u);

    constexpr auto kExpectBin = R"JSON({
        "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "closed": true
    })JSON";
    EXPECT_EQ(binJson, boost::json::parse(kExpectBin));

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
        kIndex1,
        30
    );
    auto json = toJson(ledgerHeader, false, 1u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, LedgerHeaderJsonV2)
{
    auto const ledgerHeader = createLedgerHeader(kIndex1, 30);

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
        kIndex1,
        30
    );
    auto json = toJson(ledgerHeader, false, 2u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV1)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kAccount, 30, 1, kIndex1, kIndex2);
    auto const json = toJsonWithBinaryTx(txMeta, 1);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta)));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV2)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kAccount, 30, 1, kIndex1, kIndex2);
    auto const json = toJsonWithBinaryTx(txMeta, 2);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta_blob)));
}

TEST_F(RPCHelpersTest, ParseIssue)
{
    auto const kJson = fmt::format(
        R"JSON({{
            "issuer": "{}",
            "currency": "JPY"
        }})JSON",
        kAccount2
    );
    auto issue = parseIssue(boost::json::parse(kJson).as_object());
    EXPECT_TRUE(issue.account == getAccountIdWithString(kAccount2));

    issue = parseIssue(boost::json::parse(R"JSON({"currency": "XRP"})JSON").as_object());
    EXPECT_TRUE(xrpl::isXRP(issue.currency));

    EXPECT_THROW(
        parseIssue(boost::json::parse(R"JSON({"currency": 2})JSON").as_object()), std::runtime_error
    );

    EXPECT_THROW(
        parseIssue(boost::json::parse(R"JSON({"currency": "XRP2"})JSON").as_object()),
        std::runtime_error
    );

    constexpr auto kJson2 = R"JSON({
        "issuer": "abcd",
        "currency": "JPY"
    })JSON";
    EXPECT_THROW(parseIssue(boost::json::parse(kJson2).as_object()), std::runtime_error);

    EXPECT_THROW(
        parseIssue(
            boost::json::parse(fmt::format(R"JSON({{"issuer": "{}"}})JSON", kAccount2)).as_object()
        ),
        std::runtime_error
    );
}

TEST_F(RPCHelpersTest, ParseBookValid)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const usd = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};
    auto const xrp = xrpl::Asset{xrpl::xrpIssue()};

    xrpl::MPTID mptId;
    ASSERT_TRUE(mptId.parseHex("000004C463C52827307480341125DA0577DEFC38405DBADD"));
    auto const mpt = xrpl::Asset{xrpl::MPTIssue{mptId}};

    xrpl::MPTID mptId2;
    ASSERT_TRUE(mptId2.parseHex("000004C463C52827307480341125DA0577DEFC38405DBADE"));
    auto const mpt2 = xrpl::Asset{xrpl::MPTIssue{mptId2}};

    // IOU vs XRP, no domain. Book is {in = pays, out = gets}.
    {
        auto const book = rpc::parseBook(usd, xrp, std::nullopt);
        ASSERT_TRUE(book.has_value());
        EXPECT_TRUE(book->in == usd);
        EXPECT_TRUE(book->out == xrp);
        EXPECT_FALSE(book->domain.has_value());
    }

    // MPT vs XRP.
    {
        auto const book = rpc::parseBook(xrp, mpt, std::nullopt);
        ASSERT_TRUE(book.has_value());
        EXPECT_TRUE(book->in == xrp);
        EXPECT_TRUE(book->out == mpt);
    }

    // MPT vs MPT with distinct issuances.
    {
        auto const book = rpc::parseBook(mpt, mpt2, std::nullopt);
        ASSERT_TRUE(book.has_value());
        EXPECT_TRUE(book->in == mpt);
        EXPECT_TRUE(book->out == mpt2);
    }

    // MPT vs IOU.
    {
        auto const book = rpc::parseBook(mpt, usd, std::nullopt);
        ASSERT_TRUE(book.has_value());
        EXPECT_TRUE(book->in == mpt);
        EXPECT_TRUE(book->out == usd);
    }

    // A valid domain is parsed into the book.
    {
        auto const book = rpc::parseBook(usd, xrp, std::string{kIndex1});
        ASSERT_TRUE(book.has_value());
        ASSERT_TRUE(book->domain.has_value());
        xrpl::uint256 expectedDomain;
        ASSERT_TRUE(expectedDomain.parseHex(kIndex1));
        EXPECT_EQ(book->domain, std::optional<xrpl::uint256>{expectedDomain});
    }
}

TEST_F(RPCHelpersTest, ParseBookIssuerErrors)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const validGets = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};
    auto const validPays = xrpl::Asset{xrpl::xrpIssue()};

    // taker_pays: XRP currency must not carry an issuer.
    {
        auto const book = rpc::parseBook(
            xrpl::Asset{xrpl::Issue{xrpl::xrpCurrency(), account}}, validGets, std::nullopt
        );
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcSrcIsrMalformed});
        EXPECT_EQ(
            book.error().message,
            "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        );
    }

    // taker_pays: non-XRP currency must not have an XRP issuer.
    {
        auto const book = rpc::parseBook(
            xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), xrpl::xrpAccount()}},
            validGets,
            std::nullopt
        );
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcSrcIsrMalformed});
        EXPECT_EQ(
            book.error().message, "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        );
    }

    // taker_gets: XRP currency must not carry an issuer.
    {
        auto const book = rpc::parseBook(
            validPays, xrpl::Asset{xrpl::Issue{xrpl::xrpCurrency(), account}}, std::nullopt
        );
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcDstIsrMalformed});
        EXPECT_EQ(
            book.error().message,
            "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        );
    }

    // taker_gets: non-XRP currency must not have an XRP issuer.
    {
        auto const book = rpc::parseBook(
            validPays,
            xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), xrpl::xrpAccount()}},
            std::nullopt
        );
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcDstIsrMalformed});
        EXPECT_EQ(
            book.error().message, "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        );
    }

    // MPT assets skip issuer-consistency checks (the issuer is encoded in the issuance id).
    {
        xrpl::MPTID mptId;
        ASSERT_TRUE(mptId.parseHex("000004C463C52827307480341125DA0577DEFC38405DBADD"));
        auto const book =
            rpc::parseBook(xrpl::Asset{xrpl::MPTIssue{mptId}}, validPays, std::nullopt);
        ASSERT_TRUE(book.has_value());
    }
}

TEST_F(RPCHelpersTest, ParseBookBadMarket)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const usd = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};

    // Identical IOU assets on both sides.
    {
        auto const book = rpc::parseBook(usd, usd, std::nullopt);
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcBadMarket});
        // badMarket carries no explicit message; it renders from the code's default, matching
        // rippled's "No such market.".
        EXPECT_EQ(rpc::makeError(book.error()).at("error_message").as_string(), "No such market.");
    }

    // Identical MPT assets on both sides.
    {
        xrpl::MPTID mptId;
        ASSERT_TRUE(mptId.parseHex("000004C463C52827307480341125DA0577DEFC38405DBADD"));
        auto const mpt = xrpl::Asset{xrpl::MPTIssue{mptId}};
        auto const book = rpc::parseBook(mpt, mpt, std::nullopt);
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcBadMarket});
        // badMarket carries no explicit message; it renders from the code's default, matching
        // rippled's "No such market.".
        EXPECT_EQ(rpc::makeError(book.error()).at("error_message").as_string(), "No such market.");
    }
}

TEST_F(RPCHelpersTest, ParseBookDomainMalformed)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const usd = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};
    auto const xrp = xrpl::Asset{xrpl::xrpIssue()};

    auto const book = rpc::parseBook(usd, xrp, std::string{"notavalidhex"});
    ASSERT_FALSE(book.has_value());
    EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcDomainMalformed});
    EXPECT_EQ(book.error().message, "Unable to parse domain.");
}

TEST_F(RPCHelpersTest, ParseBookDomainCheckedBeforeBadMarket)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const usd = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};

    // Identical assets (badMarket) AND a malformed domain: rippled's doBookOffers parses the
    // domain before the "taker_gets same as taker_pays" check, so the domain error wins.
    auto const book = rpc::parseBook(usd, usd, std::string{"notavalidhex"});
    ASSERT_FALSE(book.has_value());
    EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcDomainMalformed});
    EXPECT_EQ(book.error().message, "Unable to parse domain.");
}

TEST_F(RPCHelpersTest, ParseBookCurrencyOverloadDelegates)
{
    auto const account = getAccountIdWithString(kAccount);

    // A valid book built via the currency/issuer overload matches the asset-based result.
    {
        auto const book = rpc::parseBook(
            xrpl::toCurrency("USD"), account, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        );
        ASSERT_TRUE(book.has_value());
        auto const expectedIn = xrpl::Asset{xrpl::Issue{xrpl::toCurrency("USD"), account}};
        auto const expectedOut = xrpl::Asset{xrpl::xrpIssue()};
        EXPECT_TRUE(book->in == expectedIn);
        EXPECT_TRUE(book->out == expectedOut);
    }

    // Errors propagate from the delegated asset-based overload.
    {
        auto const book = rpc::parseBook(
            xrpl::toCurrency("USD"), account, xrpl::toCurrency("USD"), account, std::nullopt
        );
        ASSERT_FALSE(book.has_value());
        EXPECT_TRUE(book.error().code == CombinedError{RippledError::RpcBadMarket});
        // badMarket carries no explicit message; it renders from the code's default, matching
        // rippled's "No such market.".
        EXPECT_EQ(rpc::makeError(book.error()).at("error_message").as_string(), "No such market.");
    }
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_BlobDoesNotExist)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuerKey = xrpl::keylet::account(account);

    // returns empty blob
    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLedgerSeqObject, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    runSpawn([&](boost::asio::yield_context yield) {
        // return false: blob doesn't exist
        EXPECT_FALSE(fetchAndCheckAnyFlagsExists(
            *backend_, kLedgerSeqObject, issuerKey, {xrpl::lsfHighDeepFreeze}, yield
        ));
    });
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_AccountWithCorrectFlag)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuerKey = xrpl::keylet::account(account);

    // create account with highDeepFreeze Flag
    auto const accountObject =
        createAccountRootObject(kAccount, xrpl::lsfHighDeepFreeze, 1, 10, 2, kTxnId, 3);

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLedgerSeqObject, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // returns true: accountObject has the highDeepFreeze flag
        EXPECT_TRUE(fetchAndCheckAnyFlagsExists(
            *backend_, kLedgerSeqObject, issuerKey, {xrpl::lsfHighDeepFreeze}, yield
        ));
    });
}

TEST_F(RPCHelpersTest, FetchAndCheckAnyFlagExists_TrustLineIsFrozenAndCheckFreezeFlag)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuerKey = xrpl::keylet::account(account);

    // create account with lowDeepFreeze Flag
    auto const accountObject =
        createAccountRootObject(kAccount, xrpl::lsfLowDeepFreeze, 1, 10, 2, kTxnId, 3);

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey.key, kLedgerSeqObject, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // returns false: accountObject has the lowDeepFreeze flag
        EXPECT_FALSE(fetchAndCheckAnyFlagsExists(
            *backend_, kLedgerSeqObject, issuerKey, {xrpl::lsfHighDeepFreeze}, yield
        ));
    });
}

TEST_F(RPCHelpersTest, ParseDelegateType)
{
    auto result = parseDelegateType(boost::json::value("authorizer"));
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result, DelegateFilter::Role::Authorizer);

    result = parseDelegateType(boost::json::value("actor"));
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result, DelegateFilter::Role::Actor);

    // invalid types
    result = parseDelegateType(boost::json::value("invalid_type"));
    EXPECT_FALSE(result.has_value());

    result = parseDelegateType(boost::json::value(123));
    EXPECT_FALSE(result.has_value());

    result = parseDelegateType(boost::json::value(true));
    EXPECT_FALSE(result.has_value());
}

TEST_F(RPCHelpersTest, ParseDelegateFilter_Success)
{
    // only delegate agent is valid
    {
        auto const jsonStr = R"JSON({
            "delegate_filter": "authorizer"
        })JSON";
        auto const json = boost::json::parse(jsonStr).as_object();

        auto const result = parseDelegateFilter(json);
        ASSERT_TRUE(result.has_value());
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        EXPECT_EQ(result->delegateType, DelegateFilter::Role::Authorizer);
        EXPECT_FALSE(result->counterParty.has_value());
        // NOLINTEND(bugprone-unchecked-optional-access)
    }

    // delegate agent + counter_party is valid
    {
        auto const jsonStr = fmt::format(
            R"JSON({{
                "delegate_filter": "actor",
                "counter_party": "{}"
            }})JSON",
            kAccount2
        );
        auto const json = boost::json::parse(jsonStr).as_object();

        auto const result = parseDelegateFilter(json);
        ASSERT_TRUE(result.has_value());
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        EXPECT_EQ(result->delegateType, DelegateFilter::Role::Actor);
        ASSERT_TRUE(result->counterParty.has_value());
        EXPECT_EQ(*result->counterParty, kAccount2);
        // NOLINTEND(bugprone-unchecked-optional-access)
    }
}

TEST_F(RPCHelpersTest, ParseDelegateFilter_Failures)
{
    // Missing required "delegate_filter" key
    {
        auto const jsonStr = fmt::format(
            R"JSON({{
                "counter_party": "{}"
            }})JSON",
            kAccount2
        );
        auto const json = boost::json::parse(jsonStr).as_object();
        EXPECT_FALSE(parseDelegateFilter(json).has_value());
    }

    // "delegate_filter" is not a string (it's an integer)
    {
        auto const jsonStr = R"JSON({
            "delegate_filter": 123
        })JSON";
        auto const json = boost::json::parse(jsonStr).as_object();
        EXPECT_FALSE(parseDelegateFilter(json).has_value());
    }

    // "delegate_filter" is a string but invalid value
    {
        auto const jsonStr = R"JSON({
            "delegate_filter": "random_string"
        })JSON";
        auto const json = boost::json::parse(jsonStr).as_object();
        EXPECT_FALSE(parseDelegateFilter(json).has_value());
    }

    // "counter_party" exists but is not a string (it's a number)
    {
        auto const jsonStr = R"JSON({
            "delegate_filter": "authorizer",
            "counter_party": 9999
        })JSON";
        auto const json = boost::json::parse(jsonStr).as_object();
        EXPECT_FALSE(parseDelegateFilter(json).has_value());
    }
}

namespace {

xrpl::MPTIssue
makeMptIssue()
{
    xrpl::MPTID mptId;
    [[maybe_unused]] auto const parsed =
        mptId.parseHex("000004C463C52827307480341125DA0577DEFC38405DBADD");
    return xrpl::MPTIssue{mptId};
}

}  // namespace

TEST_F(RPCHelpersTest, AccountHoldsMPT_IssuerReturnsAvailableCapacity)
{
    auto const mptIssue = makeMptIssue();
    auto const issuer = mptIssue.getIssuer();
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // MaximumAmount = 1000, OutstandingAmount = 100 -> available = 900.
    auto const issuance = createMptIssuanceObject(
        kAccount,
        2,
        std::nullopt,
        0,
        /* outstanding */ 100,
        std::nullopt,
        std::nullopt,
        /* max */ 1000
    );
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, issuer, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), 900u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_IssuerNoIssuanceReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const issuer = mptIssue.getIssuer();
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(std::optional<xrpl::Blob>{}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, issuer, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), 0u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderReturnsBalance)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    auto const token = createMpTokenObject(kAccount2, mptIssue.getMptID(), 7);
    auto const issuance = createMptIssuanceObject(kAccount, 2, std::nullopt, 0);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), 7u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderNoTokenReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;

    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(std::optional<xrpl::Blob>{}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), 0u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderGloballyFrozenReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    auto const token = createMpTokenObject(kAccount2, mptIssue.getMptID(), 7);
    // Issuance locked -> globally frozen.
    auto const issuance = createMptIssuanceObject(kAccount, 2, std::nullopt, xrpl::lsfMPTLocked);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // Frozen -> zero when zeroIfFrozen is set; balance otherwise.
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield)
                .mpt()
                .value(),
            0u
        );
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, false, yield)
                .mpt()
                .value(),
            7u
        );
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderIndividuallyFrozenReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // Token itself locked -> individually frozen.
    auto const token = createMpTokenObject(kAccount2, mptIssue.getMptID(), 7, xrpl::lsfMPTLocked);
    auto const issuance = createMptIssuanceObject(kAccount, 2, std::nullopt, 0);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield)
                .mpt()
                .value(),
            0u
        );
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderUnauthorizedReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // Issuance requires auth; the holder's token is NOT authorized -> spendable balance is zero,
    // even though the freeze flags are clear. Mirrors rippled's ZeroIfUnauthorized.
    auto const token = createMpTokenObject(kAccount2, mptIssue.getMptID(), 7);
    auto const issuance =
        createMptIssuanceObject(kAccount, 2, std::nullopt, xrpl::lsfMPTRequireAuth);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // Unauthorized: zero regardless of zeroIfFrozen.
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield)
                .mpt()
                .value(),
            0u
        );
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, false, yield)
                .mpt()
                .value(),
            0u
        );
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderAuthorizedReturnsBalance)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // Issuance requires auth and the holder IS authorized -> full balance.
    auto const token =
        createMpTokenObject(kAccount2, mptIssue.getMptID(), 7, xrpl::lsfMPTAuthorized);
    auto const issuance =
        createMptIssuanceObject(kAccount, 2, std::nullopt, xrpl::lsfMPTRequireAuth);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield)
                .mpt()
                .value(),
            7u
        );
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_IssuerOutstandingExceedsMaxReturnsZero)
{
    auto const mptIssue = makeMptIssue();
    auto const issuer = mptIssue.getIssuer();
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // OutstandingAmount (500) > MaximumAmount (100) -> available must clamp to zero.
    auto const issuance = createMptIssuanceObject(
        kAccount,
        2,
        std::nullopt,
        0,
        /* outstanding */ 500,
        std::nullopt,
        std::nullopt,
        /* max */ 100
    );
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, issuer, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), 0u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_IssuerDefaultMaxWhenFieldAbsent)
{
    auto const mptIssue = makeMptIssue();
    auto const issuer = mptIssue.getIssuer();
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // maxAmount = nullopt -> sfMaximumAmount field is absent -> code uses xrpl::kMaxMpTokenAmount.
    // OutstandingAmount = 50 -> available = kMaxMpTokenAmount - 50.
    auto const issuance = createMptIssuanceObject(
        kAccount,
        2,
        std::nullopt,
        0,
        /* outstanding */ 50,
        std::nullopt,
        std::nullopt,
        /* maxAmount */ std::nullopt
    );
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(issuance.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const held =
            accountHoldsMPT(*backend_, kLedgerSeqObject, issuer, mptIssue, true, yield);
        EXPECT_EQ(held.mpt().value(), xrpl::kMaxMpTokenAmount - 50u);
    });
}

TEST_F(RPCHelpersTest, AccountHoldsMPT_HolderMissingIssuanceReturnsBalance)
{
    auto const mptIssue = makeMptIssue();
    auto const holder = getAccountIdWithString(kAccount2);
    auto const tokenKey = xrpl::keylet::mptoken(mptIssue.getMptID(), holder).key;
    auto const issuanceKey = xrpl::keylet::mptokenIssuance(mptIssue.getMptID()).key;

    // Token exists with a balance, but the issuance object is missing.
    // Freeze and require-auth checks are skipped; the raw sfMPTAmount is returned.
    auto const token = createMpTokenObject(kAccount2, mptIssue.getMptID(), 42);
    ON_CALL(*backend_, doFetchLedgerObject(tokenKey, kLedgerSeqObject, _))
        .WillByDefault(Return(token.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKey, kLedgerSeqObject, _))
        .WillByDefault(Return(std::optional<xrpl::Blob>{}));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, true, yield)
                .mpt()
                .value(),
            42u
        );
        EXPECT_EQ(
            accountHoldsMPT(*backend_, kLedgerSeqObject, holder, mptIssue, false, yield)
                .mpt()
                .value(),
            42u
        );
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_TrustLineIsDeepFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const account2 = getAccountIdWithString(kAccount2);

    // create a trustline between account and account2 and is deep frozen
    auto const trustLineKey =
        xrpl::keylet::trustLine(account, account2, xrpl::Currency{kCurrency}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfLowDeepFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(isDeepFrozen(
            *backend_, kLedgerSeqObject, account, xrpl::Currency{kCurrency}, account2, yield
        ));
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_TrustLineIsNotDeepFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const account2 = getAccountIdWithString(kAccount2);

    // create a trustline between account and account2 that is frozen (NOT DeepFrozen)
    auto const trustLineKey =
        xrpl::keylet::trustLine(account, account2, xrpl::Currency{kCurrency}).key;
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfLowFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isDeepFrozen(
            *backend_, kLedgerSeqObject, account, xrpl::Currency{kCurrency}, account2, yield
        ));
    });
}

TEST_F(RPCHelpersTest, isDeepFrozen_IssuerAndAccountIsSameWillNotBeDeepFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);

    auto const trustLineKey =
        xrpl::keylet::trustLine(account, issuer, xrpl::Currency{kCurrency}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfLowDeepFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        // both accounts are same so trustline is not deep frozen
        EXPECT_FALSE(isDeepFrozen(
            *backend_, kLedgerSeqObject, account, xrpl::Currency{kCurrency}, account, yield
        ));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerAccountIsGlobalFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);

    auto const accountObject =
        createAccountRootObject(kAccount2, xrpl::lsfGlobalFreeze, 1, 10, 2, kTxnId, 3);
    auto const issuerKey = xrpl::keylet::account(issuer).key;

    ON_CALL(*backend_, doFetchLedgerObject(issuerKey, kLedgerSeqObject, _))
        .WillByDefault(Return(accountObject.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(
            isFrozen(*backend_, kLedgerSeqObject, account, xrpl::Currency{kCurrency}, issuer, yield)
        );
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerAndAccountIsSameWillNotBeFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);

    auto const trustLineKey =
        xrpl::keylet::trustLine(account, issuer, xrpl::Currency{kCurrency}).key;
    auto const trustlineDeepFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineDeepFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isFrozen(
            *backend_, kLedgerSeqObject, account, xrpl::Currency{kCurrency}, account, yield
        ));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerTrustLineIsFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);
    xrpl::Currency const currency{kCurrency};

    auto const trustLineKey = xrpl::keylet::trustLine(account, issuer, currency).key;

    // issuer is higher than account, so the correct flag to set is High freeze
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_TRUE(isFrozen(*backend_, kLedgerSeqObject, account, currency, issuer, yield));
    });
}

TEST_F(RPCHelpersTest, isFrozen_IssuerWithLowFreezeIsNotFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);
    xrpl::Currency const currency{kCurrency};

    auto const trustLineKey = xrpl::keylet::trustLine(account, issuer, currency).key;

    // issuer is higher than account, but the flag set here is low freeze
    auto const trustlineFrozen = createRippleStateLedgerObject(
        "USD", kAccount, 8, kAccount, 1000, kAccount2, 2000, kIndex1, 2, xrpl::lsfLowFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustlineFrozen.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_FALSE(isFrozen(*backend_, kLedgerSeqObject, account, currency, issuer, yield));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_TrustLineNotfrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);
    xrpl::Currency const currency{kCurrency};

    auto const trustLineKey = xrpl::keylet::trustLine(account, issuer, currency).key;
    auto const trustLine = createRippleStateLedgerObject(
        kCurrency, kAccount2, 500, kAccount, 1000, kAccount2, 1000, kTxnId, 1, 0
    );

    EXPECT_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillOnce(Return(trustLine.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            kLedgerSeqObject,
            account,
            currency,
            issuer,
            false,
            yield
        );
        // Check issuer has a balance of 500
        EXPECT_EQ(result, xrpl::STAmount(getIssue(kCurrency, kAccount2), 500));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_NoTrustLine)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);
    xrpl::Currency const currency{kCurrency};

    auto const key = xrpl::keylet::trustLine(account, issuer, currency).key;

    // return no trustline found
    EXPECT_CALL(*backend_, doFetchLedgerObject(key, kLedgerSeqObject, _))
        .WillOnce(Return(std::nullopt));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            kLedgerSeqObject,
            account,
            currency,
            issuer,
            false,
            yield
        );
        // balance is 0 as trustline is frozen
        EXPECT_EQ(result, xrpl::STAmount(getIssue(kCurrency, kAccount2), 0));
    });
}

TEST_F(RPCHelpersTest, AccountHolds_TrustLineButFrozen)
{
    auto const account = getAccountIdWithString(kAccount);
    auto const issuer = getAccountIdWithString(kAccount2);
    xrpl::Currency const currency{kCurrency};

    // balance of 500, but trustline is frozen
    auto const trustLineKey = xrpl::keylet::trustLine(account, issuer, currency).key;

    auto const trustLine = createRippleStateLedgerObject(
        kCurrency, kAccount2, 500, kAccount, 1000, kAccount2, 1000, kTxnId, 1, xrpl::lsfHighFreeze
    );

    ON_CALL(*backend_, doFetchLedgerObject(trustLineKey, kLedgerSeqObject, _))
        .WillByDefault(Return(trustLine.getSerializer().peekData()));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            kLedgerSeqObject,
            account,
            currency,
            issuer,
            true,
            yield
        );
        EXPECT_EQ(result, xrpl::STAmount(getIssue(kCurrency, kAccount2), 0));
    });
}

TEST_F(RPCHelpersTest, AccountHoldsFixLPTAmendmentDisabled)
{
    auto ammAccount = getAccountIdWithString(kAmmAccount);
    auto account = getAccountIdWithString(kAccount);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLptokenCurrency, kAmmAccount, 100, kAccount, 100, kAmmAccount, 100, kTxnId, 3
    );
    auto const lptRippleStateKk =
        xrpl::keylet::trustLine(ammAccount, account, xrpl::toCurrency(kLptokenCurrency)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    auto const ammID = xrpl::uint256{kAmmId};
    auto const ammAccountKk = xrpl::keylet::account(ammAccount).key;
    auto const ammAccountRoot =
        createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2, 0, ammID);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .WillOnce(Return(ammAccountRoot.getSerializer().peekData()));

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(Return(false));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            xrpl::toCurrency(kLptokenCurrency),
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
    auto account = getAccountIdWithString(kAccount);
    auto account2 = getAccountIdWithString(kAccount2);

    auto const usdRippleState = createRippleStateLedgerObject(
        "USD", kAccount2, 100, kAccount, 100, kAccount2, 100, kTxnId, 3
    );
    auto const usdRippleStateKk =
        xrpl::keylet::trustLine(account2, account, xrpl::toCurrency("USD")).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(usdRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(usdRippleState.getSerializer().peekData()));

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(Return(true));

    auto const account2Kk = xrpl::keylet::account(account2).key;
    auto const account2Root = createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2, 0);

    EXPECT_CALL(*backend_, doFetchLedgerObject(account2Kk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(account2Root.getSerializer().peekData()));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            xrpl::toCurrency("USD"),
            account2,
            true,
            yield
        );
        EXPECT_EQ(ret.mantissa(), 1000000000000000);
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, AccountHoldsLPTokenAsset1Frozen)
{
    auto ammAccount = getAccountIdWithString(kAmmAccount);
    auto account = getAccountIdWithString(kAccount);
    auto issuer = getAccountIdWithString(kIssuer);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLptokenCurrency, kAmmAccount, 100, kAccount, 100, kAmmAccount, 100, kTxnId, 3
    );
    auto const lptRippleStateKk =
        xrpl::keylet::trustLine(ammAccount, account, xrpl::toCurrency(kLptokenCurrency)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(Return(true));

    auto const ammID = xrpl::uint256{kAmmId};
    auto const ammAccountKk = xrpl::keylet::account(ammAccount).key;
    auto const ammAccountRoot =
        createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm =
        createAmmObject(kAmmAccount, "USD", kIssuer, "XRP", xrpl::toBase58(xrpl::xrpAccount()));
    EXPECT_CALL(
        *backend_, doFetchLedgerObject(xrpl::keylet::amm(ammID).key, testing::_, testing::_)
    )
        .Times(1)
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = xrpl::keylet::account(issuer).key;
    auto const issuerAccountRoot =
        createAccountRootObject(kIssuer, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            xrpl::toCurrency(kLptokenCurrency),
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
    auto ammAccount = getAccountIdWithString(kAmmAccount);
    auto account = getAccountIdWithString(kAccount);
    auto issuer = getAccountIdWithString(kIssuer);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLptokenCurrency, kAmmAccount, 100, kAccount, 100, kAmmAccount, 100, kTxnId, 3
    );
    auto const lptRippleStateKk =
        xrpl::keylet::trustLine(ammAccount, account, xrpl::toCurrency(kLptokenCurrency)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_)).Times(2);
    ON_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .WillByDefault(testing::Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(testing::Return(true));

    auto const ammID = xrpl::uint256{kAmmId};
    auto const ammAccountKk = xrpl::keylet::account(ammAccount).key;
    auto const ammAccountRoot =
        createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm =
        createAmmObject(kAmmAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "USD", kIssuer);
    EXPECT_CALL(
        *backend_, doFetchLedgerObject(xrpl::keylet::amm(ammID).key, testing::_, testing::_)
    )
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = xrpl::keylet::account(issuer).key;
    auto const issuerAccountRoot =
        createAccountRootObject(kIssuer, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            xrpl::toCurrency(kLptokenCurrency),
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
    auto ammAccount = getAccountIdWithString(kAmmAccount);
    auto account = getAccountIdWithString(kAccount);
    auto issuer = getAccountIdWithString(kIssuer);

    auto const lptRippleState = createRippleStateLedgerObject(
        kLptokenCurrency, kAmmAccount, 100, kAccount, 100, kAmmAccount, 100, kTxnId, 3
    );
    auto const lptRippleStateKk =
        xrpl::keylet::trustLine(ammAccount, account, xrpl::toCurrency(kLptokenCurrency)).key;

    // trustline fetched twice. once in accountHolds and once in isFrozen
    EXPECT_CALL(*backend_, doFetchLedgerObject(lptRippleStateKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(lptRippleState.getSerializer().peekData()));

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(Return(true));

    auto const ammID = xrpl::uint256{kAmmId};
    auto const ammAccountKk = xrpl::keylet::account(ammAccount).key;
    auto const ammAccountRoot =
        createAccountRootObject(kAmmAccount, 0, 2, 200, 2, kIndex1, 2, 0, ammID);

    // accountroot fetched twice, once in isFrozen, once in accountHolds
    EXPECT_CALL(*backend_, doFetchLedgerObject(ammAccountKk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(Return(ammAccountRoot.getSerializer().peekData()));

    auto const amm =
        createAmmObject(kAmmAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "USD", kIssuer);
    EXPECT_CALL(
        *backend_, doFetchLedgerObject(xrpl::keylet::amm(ammID).key, testing::_, testing::_)
    )
        .WillOnce(Return(amm.getSerializer().peekData()));

    auto const issuerKk = xrpl::keylet::account(issuer).key;
    auto const issuerAccountRoot = createAccountRootObject(kIssuer, 0, 2, 200, 2, kIndex1, 2, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject(issuerKk, testing::_, testing::_))
        .WillOnce(Return(issuerAccountRoot.getSerializer().peekData()));

    auto const usdRippleState =
        createRippleStateLedgerObject("USD", kIssuer, 100, kAccount, 100, kIssuer, 100, kTxnId, 3);
    auto const usdRippleStateKk =
        xrpl::keylet::trustLine(issuer, account, xrpl::toCurrency("USD")).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject(usdRippleStateKk, testing::_, testing::_))
        .WillOnce(Return(usdRippleState.getSerializer().peekData()));

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto ret = accountHolds(
            *backend_,
            *mockAmendmentCenterPtr_,
            0,
            account,
            xrpl::toCurrency(kLptokenCurrency),
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
        {
            .testName = "ledgerEntry",
            .method = "ledger_entry",
            .testJson = R"JSON({"type": false})JSON",
            .expected = false,
        },
        {
            .testName = "featureVetoedTrue",
            .method = "feature",
            .testJson = R"JSON({"vetoed": true, "feature": "foo"})JSON",
            .expected = true,
        },
        {
            .testName = "featureVetoedFalse",
            .method = "feature",
            .testJson = R"JSON({"vetoed": false, "feature": "foo"})JSON",
            .expected = true,
        },
        {
            .testName = "featureVetoedIsStr",
            .method = "feature",
            .testJson = R"JSON({"vetoed": "String"})JSON",
            .expected = true,
        },
        {
            .testName = "ledger",
            .method = "ledger",
            .testJson = R"JSON({})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerWithType",
            .method = "ledger",
            .testJson = R"JSON({"type": "fee"})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullTrue",
            .method = "ledger",
            .testJson = R"JSON({"full": true})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullFalse",
            .method = "ledger",
            .testJson = R"JSON({"full": false})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsStr",
            .method = "ledger",
            .testJson = R"JSON({"full": "String"})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullIsEmptyStr",
            .method = "ledger",
            .testJson = R"JSON({"full": ""})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsNumber1",
            .method = "ledger",
            .testJson = R"JSON({"full": 1})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullIsNumber0",
            .method = "ledger",
            .testJson = R"JSON({"full": 0})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsNull",
            .method = "ledger",
            .testJson = R"JSON({"full": null})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsFloat0",
            .method = "ledger",
            .testJson = R"JSON({"full": 0.0})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsFloat1",
            .method = "ledger",
            .testJson = R"JSON({"full": 0.1})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullIsArray",
            .method = "ledger",
            .testJson = R"JSON({"full": [1]})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullIsEmptyArray",
            .method = "ledger",
            .testJson = R"JSON({"full": []})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerFullIsObject",
            .method = "ledger",
            .testJson = R"JSON({"full": {"key": 1}})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerFullIsEmptyObject",
            .method = "ledger",
            .testJson = R"JSON({"full": {}})JSON",
            .expected = false,
        },

        {
            .testName = "ledgerAccountsTrue",
            .method = "ledger",
            .testJson = R"JSON({"accounts": true})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsFalse",
            .method = "ledger",
            .testJson = R"JSON({"accounts": false})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsStr",
            .method = "ledger",
            .testJson = R"JSON({"accounts": "String"})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsIsEmptyStr",
            .method = "ledger",
            .testJson = R"JSON({"accounts": ""})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsNumber1",
            .method = "ledger",
            .testJson = R"JSON({"accounts": 1})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsIsNumber0",
            .method = "ledger",
            .testJson = R"JSON({"accounts": 0})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsNull",
            .method = "ledger",
            .testJson = R"JSON({"accounts": null})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsFloat0",
            .method = "ledger",
            .testJson = R"JSON({"accounts": 0.0})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsFloat1",
            .method = "ledger",
            .testJson = R"JSON({"accounts": 0.1})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsIsArray",
            .method = "ledger",
            .testJson = R"JSON({"accounts": [1]})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsIsEmptyArray",
            .method = "ledger",
            .testJson = R"JSON({"accounts": []})JSON",
            .expected = false,
        },
        {
            .testName = "ledgerAccountsIsObject",
            .method = "ledger",
            .testJson = R"JSON({"accounts": {"key": 1}})JSON",
            .expected = true,
        },
        {
            .testName = "ledgerAccountsIsEmptyObject",
            .method = "ledger",
            .testJson = R"JSON({"accounts": {}})JSON",
            .expected = false,
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    IsAdminCmdTest,
    IsAdminCmdParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(IsAdminCmdParameterTest, Test)
{
    auto const testBundle = GetParam();
    EXPECT_EQ(
        isAdminCmd(testBundle.method, boost::json::parse(testBundle.testJson).as_object()),
        testBundle.expected
    );
}

struct RPCHelpersLogDurationTestBundle {
    std::string testName;
    std::chrono::milliseconds duration;
    std::string expectedLogLevel;
    bool expectDuration;
};

struct RPCHelpersLogDurationTest : LoggerFixture,
                                   testing::WithParamInterface<RPCHelpersLogDurationTestBundle> {
    boost::json::object const request = {
        {"method", "account_info"},
        {"params",
         boost::json::array{boost::json::object{
             {"account", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
             {"secret", "should be deleted"}
         }}}
    };
    util::TagDecoratorFactory tagFactory{util::config::ClioConfigDefinition{
        {"log.tag_style",
         util::config::ConfigValue{util::config::ConfigType::String}.defaultValue("none")}
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
        std::string const durationStr =
            std::to_string(GetParam().duration.count()) + " milliseconds";
        EXPECT_NE(output.find(durationStr), std::string::npos);
    }

    EXPECT_NE(output.find("account_info"), std::string::npos);
    EXPECT_EQ(output.find("should be deleted"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    RPCHelpersLogDurationTests,
    RPCHelpersLogDurationTest,
    testing::Values(
        RPCHelpersLogDurationTestBundle{
            "ShortDurationLogsAsInfo",
            std::chrono::milliseconds(500),
            "inf:RPC ",
            true
        },
        RPCHelpersLogDurationTestBundle{
            "MediumDurationLogsAsWarning",
            std::chrono::milliseconds(5000),
            "war:RPC ",
            true
        },
        RPCHelpersLogDurationTestBundle{
            "LongDurationLogsAsError",
            std::chrono::milliseconds(15000),
            "err:RPC ",
            true
        }
    ),
    tests::util::kNameGenerator
);
