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
#include <rpc/handlers/NFTInfo.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto NFTID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr static auto NFTID2 = "00081388319F12E15BCA13E1B933BF4C99C8E1BBC36BD4910A85D52F00000022";

class RPCNFTInfoHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCNFTInfoHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
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

TEST_F(RPCNFTInfoHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
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

TEST_F(RPCNFTInfoHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
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
TEST_F(RPCNFTInfoHandlerTest, NFTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
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
TEST_F(RPCNFTInfoHandlerTest, NFTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
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
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerHash)
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
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerStringIndex)
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
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerIntIndex)
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
            "ledger_index": 4
        }})",
        NFTID));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerHash2)
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
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_index": "31"
        }})",
        NFTID));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case nft does not exist
TEST_F(RPCNFTInfoHandlerTest, NonExistNFT)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch nft return emtpy
    ON_CALL(*rawBackendPtr, fetchNFT).WillByDefault(Return(std::optional<NFT>{}));
    EXPECT_CALL(*rawBackendPtr, fetchNFT(ripple::uint256{NFTID}, 30, _)).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        NFTID,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "NFT not found");
    });
}

// normal case when only provide nft_id
TEST_F(RPCNFTInfoHandlerTest, DefaultParameters)
{
    constexpr static auto currentOutput = R"({
        "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
        "ledger_index": 30,
        "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "is_burned": false,
        "flags": 1,
        "transfer_fee": 0,
        "issuer": "rGJUF4PvVkMNxG6Bg6AKg3avhrtQyAffcm",
        "nft_taxon": 0,
        "nft_serial": 4,
        "uri": "757269",
        "validated": true
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(CreateNFT(NFTID, ACCOUNT, ledgerInfo.seq));
    ON_CALL(*rawBackendPtr, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*rawBackendPtr, fetchNFT(ripple::uint256{NFTID}, 30, _)).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

// nft is burned -> should not omit uri
TEST_F(RPCNFTInfoHandlerTest, BurnedNFT)
{
    constexpr static auto currentOutput = R"({
        "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
        "ledger_index": 30,
        "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "is_burned": true,
        "flags": 1,
        "transfer_fee": 0,
        "issuer": "rGJUF4PvVkMNxG6Bg6AKg3avhrtQyAffcm",
        "nft_taxon": 0,
        "nft_serial": 4,
        "uri": "757269",
        "validated": true
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft =
        std::make_optional<NFT>(CreateNFT(NFTID, ACCOUNT, ledgerInfo.seq, ripple::Blob{'u', 'r', 'i'}, true));
    ON_CALL(*rawBackendPtr, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*rawBackendPtr, fetchNFT(ripple::uint256{NFTID}, 30, _)).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

// nft is not burned and uri is not available -> should specify null
TEST_F(RPCNFTInfoHandlerTest, NotBurnedNFTWithoutURI)
{
    constexpr static auto currentOutput = R"({
        "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
        "ledger_index": 30,
        "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "is_burned": false,
        "flags": 1,
        "transfer_fee": 0,
        "issuer": "rGJUF4PvVkMNxG6Bg6AKg3avhrtQyAffcm",
        "nft_taxon": 0,
        "nft_serial": 4,
        "uri": "",
        "validated": true
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(CreateNFT(NFTID, ACCOUNT, ledgerInfo.seq, ripple::Blob{}));
    ON_CALL(*rawBackendPtr, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*rawBackendPtr, fetchNFT(ripple::uint256{NFTID}, 30, _)).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        NFTID));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

// check taxon field, transfer fee and serial
TEST_F(RPCNFTInfoHandlerTest, NFTWithExtraFieldsSet)
{
    constexpr static auto currentOutput = R"({
        "nft_id": "00081388319F12E15BCA13E1B933BF4C99C8E1BBC36BD4910A85D52F00000022",
        "ledger_index": 30,
        "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "is_burned": false,
        "flags": 8,
        "transfer_fee": 5000,
        "issuer": "rnX4gsB86NNrGV8xHcJ5hbR2aKtSetbuwg",
        "nft_taxon": 7826,
        "nft_serial": 34,
        "uri": "757269",
        "validated": true
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(CreateNFT(NFTID2, ACCOUNT, ledgerInfo.seq));
    ON_CALL(*rawBackendPtr, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*rawBackendPtr, fetchNFT(ripple::uint256{NFTID2}, 30, _)).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        NFTID2));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}
