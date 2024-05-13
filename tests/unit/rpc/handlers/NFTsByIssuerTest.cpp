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
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTsByIssuer.hpp"
#include "util/Fixtures.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerHeader.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto NFTID1 = "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F0000099B00000000";  // taxon 0
constexpr static auto NFTID2 = "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F16E5DA9C00000001";  // taxon 0
constexpr static auto NFTID3 = "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F5B974D9E00000004";  // taxon 1

static std::string NFT1OUT =
    R"({
        "nft_id": "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F0000099B00000000",
        "ledger_index": 29,
        "owner": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "is_burned": false,
        "uri": "757269",
        "flags": 8,
        "transfer_fee": 0,
        "issuer": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "nft_taxon": 0,
        "nft_serial": 0
    })";
static std::string NFT2OUT =
    R"({
        "nft_id": "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F16E5DA9C00000001",
        "ledger_index": 29,
        "owner": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "is_burned": false,
        "uri": "757269",
        "flags": 8,
        "transfer_fee": 0,
        "issuer": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "nft_taxon": 0,
        "nft_serial": 1
    })";
static std::string NFT3OUT =
    R"({
        "nft_id": "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F5B974D9E00000004",
        "ledger_index": 29,
        "owner": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "is_burned": false,
        "uri": "757269",
        "flags": 8,
        "transfer_fee": 0,
        "issuer": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
        "nft_taxon": 1,
        "nft_serial": 4
    })";

class RPCNFTsByIssuerHandlerTest : public HandlerBaseTest {};

TEST_F(RPCNFTsByIssuerHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "issuer": "{}", 
                "ledger_hash": "xxx"
            }})",
            ACCOUNT
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{
                "issuer": "{}", 
                "ledger_hash": 123
            }})",
            ACCOUNT
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "issuer": "{}", 
                "ledger_index": "notvalidated"
            }})",
            ACCOUNT
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: issuer invalid format, length is incorrect
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(R"({ 
            "issuer": "xxx"
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "issuerMalformed");
    });
}

// error case: issuer missing
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerMissing)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(R"({})");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Required field 'issuer' missing");
    });
}

// error case: issuer invalid format
TEST_F(RPCNFTsByIssuerHandlerTest, NFTIssuerNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const input = json::parse(R"({ 
            "issuer": 12
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "issuerNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    backend->setRange(10, 30);
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerInfo>{}));
    auto const input = json::parse(fmt::format(
        R"({{ 
            "issuer": "{}",
            "ledger_index": "4"
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    backend->setRange(10, 30);
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerInfo>{}));
    auto const input = json::parse(fmt::format(
        R"({{ 
            "issuer": "{}",
            "ledger_index": 4
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerHash2)
{
    backend->setRange(10, 30);
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 31);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTsByIssuerHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    backend->setRange(10, 30);
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "issuer": "{}",
            "ledger_index": "31"
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTsByIssuerHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// normal case when issuer does not exist or has no NFTs
TEST_F(RPCNFTsByIssuerHandlerTest, AccountNotFound)
{
    backend->setRange(10, 30);
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

// normal case when issuer has a single nft
TEST_F(RPCNFTsByIssuerHandlerTest, DefaultParameters)
{
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":50,
        "ledger_index": 30,
        "nfts": [{}],
        "validated": true
    }})",
        ACCOUNT,
        NFT1OUT
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {CreateNFT(NFTID1, ACCOUNT, 29)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(
            account, testing::Eq(std::nullopt), Const(30), testing::_, testing::Eq(std::nullopt), testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}"
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, SpecificLedgerIndex)
{
    auto const specificLedger = 20;
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":50,
        "ledger_index": {},
        "nfts": [{{
            "nft_id": "00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F0000099B00000000",
            "ledger_index": 20,
            "owner": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
            "is_burned": false,
            "uri": "757269",
            "flags": 8,
            "transfer_fee": 0,
            "issuer": "r4X6JLsBfhNK4UnquNkCxhVHKPkvbQff67",
            "nft_taxon": 0,
            "nft_serial": 0
        }}],
        "validated": true
    }})",
        ACCOUNT,
        specificLedger
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, specificLedger);
    ON_CALL(*backend, fetchLedgerBySequence(specificLedger, _)).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, specificLedger, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {CreateNFT(NFTID1, ACCOUNT, specificLedger)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(
            account, testing::Eq(std::nullopt), Const(specificLedger), testing::_, testing::Eq(std::nullopt), testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "ledger_index": {}
        }})",
        ACCOUNT,
        specificLedger
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, TaxonParameter)
{
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":50,
        "ledger_index": 30,
        "nfts": [{}],
        "validated": true,
        "nft_taxon": 0
    }})",
        ACCOUNT,
        NFT1OUT
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {CreateNFT(NFTID1, ACCOUNT, 29)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(account, testing::Optional(0), Const(30), testing::_, testing::Eq(std::nullopt), testing::_)
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "nft_taxon": 0
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, MarkerParameter)
{
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":50,
        "ledger_index": 30,
        "nfts": [{}],
        "validated": true,
        "marker":"00080000EC28C2910FD1C454A51598AAB91C8876286B2E7F5B974D9E00000004"
    }})",
        ACCOUNT,
        NFT3OUT
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {CreateNFT(NFTID3, ACCOUNT, 29)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, ripple::uint256{NFTID3}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(account, testing::_, Const(30), testing::_, testing::Eq(ripple::uint256{NFTID1}), testing::_)
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "marker": "{}"
        }})",
        ACCOUNT,
        NFTID1
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, MultipleNFTs)
{
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":50,
        "ledger_index": 30,
        "nfts": [{}, {}, {}],
        "validated": true
    }})",
        ACCOUNT,
        NFT1OUT,
        NFT2OUT,
        NFT3OUT
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {
        CreateNFT(NFTID1, ACCOUNT, 29), CreateNFT(NFTID2, ACCOUNT, 29), CreateNFT(NFTID3, ACCOUNT, 29)
    };
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(
            account, testing::Eq(std::nullopt), Const(30), testing::_, testing::Eq(std::nullopt), testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}"
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCNFTsByIssuerHandlerTest, LimitMoreThanMAx)
{
    auto const currentOutput = fmt::format(
        R"({{
        "issuer": "{}",
        "limit":100,
        "ledger_index": 30,
        "nfts": [{}],
        "validated": true
    }})",
        ACCOUNT,
        NFT1OUT
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    std::vector<NFT> const nfts = {CreateNFT(NFTID1, ACCOUNT, 29)};
    auto const account = GetAccountIDWithString(ACCOUNT);
    ON_CALL(*backend, fetchNFTsByIssuer).WillByDefault(Return(NFTsAndCursor{nfts, {}}));
    EXPECT_CALL(
        *backend,
        fetchNFTsByIssuer(
            account,
            testing::Eq(std::nullopt),
            Const(30),
            Const(NFTsByIssuerHandler::LIMIT_MAX),
            testing::Eq(std::nullopt),
            testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "issuer": "{}",
            "limit": {}
        }})",
        ACCOUNT,
        NFTsByIssuerHandler::LIMIT_MAX + 1
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{NFTsByIssuerHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output.result);
    });
}
