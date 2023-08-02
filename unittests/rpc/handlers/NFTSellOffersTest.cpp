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
#include <rpc/handlers/NFTSellOffers.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto NFTID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr static auto INDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

class RPCNFTSellOffersHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCNFTSellOffersHandlerTest, LimitNotInt)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": "xxx"
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, LimitNegative)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": -1
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, LimitZero)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 0
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "ledger_hash": "xxx"
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{
                "nft_id": "{}", 
                "ledger_hash": 123
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "ledger_index": "notvalidated"
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: nft_id invalid format, length is incorrect
TEST_F(RPCNFTSellOffersHandlerTest, NFTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "nft_id": "00080000B4F4AFC5FBCBD76873F18006173D2193467D3EE7"
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idMalformed");
    });
}

// error case: nft_id invalid format
TEST_F(RPCNFTSellOffersHandlerTest, NFTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "nft_id": 12
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCNFTSellOffersHandlerTest, NonExistLedgerViaLedgerHash)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // mock fetchLedgerByHash return empty
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        NFTID,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTSellOffersHandlerTest, NonExistLedgerViaLedgerIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_index": "4"
        }})",
        NFTID));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTSellOffersHandlerTest, NonExistLedgerViaLedgerHash2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 31);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        NFTID,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTSellOffersHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // no need to check from db, call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_index": "31"
        }})",
        NFTID));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case when nft is not found
TEST_F(RPCNFTSellOffersHandlerTest, NoNFT)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        NFTID,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "notFound");
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "marker": 9
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerNotString");
    });
}

// error case : invalid marker
// marker format in this RPC is a hex-string of a ripple::uint256.
TEST_F(RPCNFTSellOffersHandlerTest, InvalidMarker)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}",
                "marker": "123invalid"
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerMalformed");
    });
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "marker": 250
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// normal case when only provide nft_id
TEST_F(RPCNFTSellOffersHandlerTest, DefaultParameters)
{
    constexpr static auto correctOutput = R"({
        "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
        "validated": true,
        "offers": [
            {
                "nft_offer_index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                "flags": 0,
                "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "amount": "123"
            },
            {
                "nft_offer_index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                "flags": 0,
                "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "amount": "123"
            }
        ]
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_sells(ripple::uint256{NFTID});
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft sell offers
    std::vector<Blob> bbs;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    bbs.push_back(offer.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

// normal case when provided with nft_id and limit
TEST_F(RPCNFTSellOffersHandlerTest, MultipleResultsWithMarkerAndLimitOutput)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 500;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    auto idx = ripple::uint256{INDEX1};
    while (repetitions--)
    {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": 50
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), 50);
        EXPECT_EQ(output->at("limit").as_uint64(), 50);
        EXPECT_STREQ(
            output->at("marker").as_string().c_str(),
            "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353");
    });
}

// normal case when provided with nft_id, limit and marker
TEST_F(RPCNFTSellOffersHandlerTest, ResultsForInputWithMarkerAndLimit)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 500;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    auto idx = ripple::uint256{INDEX1};
    while (repetitions--)
    {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    auto const cursorSellOffer = CreateNFTSellOffer(NFTID, ACCOUNT);

    // first is nft offer object
    auto const cursor = ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353"};
    auto const first = ripple::keylet::nftoffer(cursor);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(first.key, testing::_, testing::_))
        .WillByDefault(Return(cursorSellOffer.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(first.key, testing::_, testing::_)).Times(1);

    auto const directory = ripple::keylet::nft_sells(ripple::uint256{NFTID});
    auto const startHint = 0ul;  // offer node is hardcoded to 0ul
    auto const secondKey = ripple::keylet::page(directory, startHint).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(secondKey, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(secondKey, testing::_, testing::_)).Times(3);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "marker": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353",
            "limit": 50
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), 50);
        EXPECT_EQ(output->at("limit").as_uint64(), 50);
        // marker also progressed by 50
        EXPECT_STREQ(
            output->at("marker").as_string().c_str(),
            "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC385");
    });
}

// normal case when provided with nft_id, limit and marker
// nothing left after reading remaining 50 entries
TEST_F(RPCNFTSellOffersHandlerTest, ResultsWithoutMarkerForInputWithMarkerAndLimit)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(3);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 100;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    auto idx = ripple::uint256{INDEX1};
    while (repetitions--)
    {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    auto const cursorSellOffer = CreateNFTSellOffer(NFTID, ACCOUNT);

    // first is nft offer object
    auto const cursor = ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353"};
    auto const first = ripple::keylet::nftoffer(cursor);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(first.key, testing::_, testing::_))
        .WillByDefault(Return(cursorSellOffer.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(first.key, testing::_, testing::_)).Times(1);

    auto const directory = ripple::keylet::nft_sells(ripple::uint256{NFTID});
    auto const startHint = 0ul;  // offer node is hardcoded to 0ul
    auto const secondKey = ripple::keylet::page(directory, startHint).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(secondKey, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(secondKey, testing::_, testing::_)).Times(7);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(3);

    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{
                "nft_id": "{}",
                "marker": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353",
                "limit": 50
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), 50);
        // no marker/limit to output - we read all items already
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 49
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTSellOffersHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 501
            }})",
            NFTID));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, LimitLessThanMin)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_sells(ripple::uint256{NFTID});
    auto const ownerDir =
        CreateOwnerDirLedgerObject(std::vector{NFTSellOffersHandler::LIMIT_MIN + 1, ripple::uint256{INDEX1}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft buy offers
    std::vector<Blob> bbs;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    for (auto i = 0; i < NFTSellOffersHandler::LIMIT_MIN + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": {}
        }})",
        NFTID,
        NFTSellOffersHandler::LIMIT_MIN - 1));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), NFTSellOffersHandler::LIMIT_MIN);
        EXPECT_EQ(output->at("limit").as_uint64(), NFTSellOffersHandler::LIMIT_MIN);
    });
}

TEST_F(RPCNFTSellOffersHandlerTest, LimitMoreThanMax)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_sells(ripple::uint256{NFTID});
    auto const ownerDir =
        CreateOwnerDirLedgerObject(std::vector{NFTSellOffersHandler::LIMIT_MAX + 1, ripple::uint256{INDEX1}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft buy offers
    std::vector<Blob> bbs;
    auto const offer = CreateNFTSellOffer(NFTID, ACCOUNT);
    for (auto i = 0; i < NFTSellOffersHandler::LIMIT_MAX + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": {}
        }})",
        NFTID,
        NFTSellOffersHandler::LIMIT_MAX + 1));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTSellOffersHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("offers").as_array().size(), NFTSellOffersHandler::LIMIT_MAX);
        EXPECT_EQ(output->at("limit").as_uint64(), NFTSellOffersHandler::LIMIT_MAX);
    });
}
