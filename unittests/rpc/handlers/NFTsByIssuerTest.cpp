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
#include <rpc/handlers/NFTsByIssuer.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto NFTID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
// constexpr static auto NFTID2 = "00081388319F12E15BCA13E1B933BF4C99C8E1BBC36BD4910A85D52F00000022";

class RPCNFTsByIssuerHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCNFTsByIssuerHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_issuer": "{}", 
                "ledger_hash": "xxx"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{
                "nft_issuer": "{}", 
                "ledger_hash": 123
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_issuer": "{}", 
                "ledger_index": "notvalidated"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: nft_issuer invalid format, length is incorrect
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "nft_issuer": "xxx"
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_issuerMalformed");
    });
}

// error case: nft_issuer missing
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerMissing)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(R"({})");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Required field 'nft_issuer' missing");
    });
}

// error case: nft_issuer invalid format
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "nft_issuer": 12
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_issuerNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerHash)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // mock fetchLedgerByHash return empty
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_issuer": "{}",
            "ledger_index": "4"
        }})",
        ACCOUNT));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_issuer": "{}",
            "ledger_index": 4
        }})",
        ACCOUNT));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerHash2)
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
            "nft_issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_issuer": "{}",
            "ledger_index": "31"
        }})",
        ACCOUNT));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// normal case when issuer does not exist or has no NFTs
TEST_F(RPCNFTsByIssuerHandlerTest, IssuerNotExistOrHasNoNFTs)
{
    auto const currentOutput = fmt::format(
        R"({{
            "nft_issuer": "{}",
            "ledger_index": 30,
            "nfts": [],
            "validated": true
        }})",
        ACCOUNT);
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch nfts return emtpy
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(
        *rawBackendPtr, fetchNFTsByIssuer(account, Ref(Const(std::nullopt)), Const(30), _, Ref(Const(std::nullopt)), _))
        .WillByDefault(Return(NFTsAndCursor{}));
    EXPECT_CALL(*rawBackendPtr, fetchNFTsByIssuer).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

// normal case when issuer has a single nft
TEST_F(RPCNFTsByIssuerHandlerTest, DefaultParameters)
{
    auto const currentOutput = fmt::format(
        R"({{
        "nft_issuer": "{}",
        "ledger_index": 30,
        "nfts": [{{
            "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
            "ledger_index": 29,
            "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "is_burned": false,
            "flags": 1,
            "transfer_fee": 0,
            "issuer": "rGJUF4PvVkMNxG6Bg6AKg3avhrtQyAffcm",
            "nft_taxon": 0,
            "nft_serial": 4,
            "uri": "757269"
        }}],
        "validated": true
    }})",
        ACCOUNT);
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    // fetch nfts return something
    std::vector<NFT> const nfts = {CreateNFT(NFTID, ACCOUNT, 29)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*rawBackendPtr, fetchNFTsByIssuer(account, Ref(Const(std::nullopt)), 30, _, Ref(Const(std::nullopt)), _))
        .WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(*rawBackendPtr, fetchNFTsByIssuer).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_issuer": "{}"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}
