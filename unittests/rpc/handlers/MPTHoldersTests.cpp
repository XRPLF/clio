//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
#include "rpc/handlers/MPTHolders.hpp"
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

// constexpr static auto ISSUER_ACCOUNT = "rsS8ju2jYabSKJ6uzLarAS1gEzvRQ6JAiF";
constexpr static auto HOLDER1_ACCOUNT = "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN";
constexpr static auto HOLDER2_ACCOUNT = "rEiNkzogdHEzUxPfsri5XSMqtXUixf2Yx";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto MPTID = "000004C463C52827307480341125DA0577DEFC38405B0E3E";

static std::string MPTOUT1 =
    R"({
        "account": "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN",
        "flags": 0,
        "mpt_amount": "1",
        "mptoken_index": "D137F2E5A5767A06CB7A8F060ADE442A30CFF95028E1AF4B8767E3A56877205A"
    })";

static std::string MPTOUT2 =
    R"({
        "account": "rEiNkzogdHEzUxPfsri5XSMqtXUixf2Yx",
        "flags": 0,
        "mpt_amount": "1",
        "mptoken_index": "36D91DEE5EFE4A93119A8B84C944A528F2B444329F3846E49FE921040DE17E65"
    })";

class RPCMPTHoldersHandlerTest : public HandlerBaseTest {};

TEST_F(RPCMPTHoldersHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "mpt_issuance_id": "{}", 
                "ledger_hash": "xxx"
            }})",
            MPTID
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{
                "mpt_issuance_id": "{}", 
                "ledger_hash": 123
            }})",
            MPTID
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "mpt_issuance_id": "{}", 
                "ledger_index": "notvalidated"
            }})",
            MPTID
        ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: issuer invalid format, length is incorrect
TEST_F(RPCMPTHoldersHandlerTest, MPTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(R"({ 
            "mpt_issuance_id": "xxx"
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "mpt_issuance_idMalformed");
    });
}

// error case: issuer missing
TEST_F(RPCMPTHoldersHandlerTest, MPTIDMissing)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(R"({})");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Required field 'mpt_issuance_id' missing");
    });
}

// error case: issuer invalid format
TEST_F(RPCMPTHoldersHandlerTest, MPTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const input = json::parse(R"({ 
            "mpt_issuance_id": 12
        })");
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "mpt_issuance_idNotString");
    });
}

// error case: invalid marker format
TEST_F(RPCMPTHoldersHandlerTest, MarkerInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "marker": "xxx"
        }})",
        MPTID
    ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerMalformed");
    });
}

// error case: invalid marker type
TEST_F(RPCMPTHoldersHandlerTest, MarkerNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "marker": 1
        }})",
        MPTID
    ));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}",
            "ledger_hash": "{}"
        }})",
        MPTID,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    backend->setRange(10, 30);
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerInfo>{}));
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "ledger_index": "4"
        }})",
        MPTID
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    backend->setRange(10, 30);
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(std::optional<ripple::LedgerInfo>{}));
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "ledger_index": 4
        }})",
        MPTID
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerHash2)
{
    backend->setRange(10, 30);
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 31);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "ledger_hash": "{}"
        }})",
        MPTID,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    backend->setRange(10, 30);
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "mpt_issuance_id": "{}",
            "ledger_index": "31"
        }})",
        MPTID
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend}};
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// normal case when MPT does not exist
TEST_F(RPCMPTHoldersHandlerTest, MPTNotFound)
{
    backend->setRange(10, 30);
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}",
            "ledger_hash": "{}"
        }})",
        MPTID,
        LEDGERHASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "objectNotFound");
    });
}

// normal case when mpt has one holder
TEST_F(RPCMPTHoldersHandlerTest, DefaultParameters)
{
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":50,
        "ledger_index": 30,
        "mptokens": [{}],
        "validated": true
    }})",
        MPTID,
        MPTOUT1
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = CreateMPTokenObject(HOLDER1_ACCOUNT, ripple::uint192(MPTID));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, {}}));
    EXPECT_CALL(
        *backend, fetchMPTHolders(ripple::uint192(MPTID), testing::_, testing::Eq(std::nullopt), Const(30), testing::_)
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}"
        }})",
        MPTID
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, CustomAmounts)
{
    // it's not possible to have locked_amount to be greater than mpt_amount,
    // we are simply testing the response parsing of the api
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":50,
        "ledger_index": 30,
        "mptokens": [{{
            "account": "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN",
            "flags": 0,
            "mpt_amount": "0",
            "locked_amount": "1",
            "mptoken_index": "D137F2E5A5767A06CB7A8F060ADE442A30CFF95028E1AF4B8767E3A56877205A"
        }}],
        "validated": true
    }})",
        MPTID
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = CreateMPTokenObject(HOLDER1_ACCOUNT, ripple::uint192(MPTID), 0, 1);
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, {}}));
    EXPECT_CALL(
        *backend, fetchMPTHolders(ripple::uint192(MPTID), testing::_, testing::Eq(std::nullopt), Const(30), testing::_)
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}"
        }})",
        MPTID
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, SpecificLedgerIndex)
{
    auto const specificLedger = 20;
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":50,
        "ledger_index": {},
        "mptokens": [{}],
        "validated": true
    }})",
        MPTID,
        specificLedger,
        MPTOUT1
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, specificLedger);
    ON_CALL(*backend, fetchLedgerBySequence(specificLedger, _)).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, specificLedger, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = CreateMPTokenObject(HOLDER1_ACCOUNT, ripple::uint192(MPTID));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, {}}));
    EXPECT_CALL(
        *backend,
        fetchMPTHolders(
            ripple::uint192(MPTID), testing::_, testing::Eq(std::nullopt), Const(specificLedger), testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}",
            "ledger_index": {}
        }})",
        MPTID,
        specificLedger
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, MarkerParameter)
{
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":50,
        "ledger_index": 30,
        "mptokens": [{}],
        "validated": true,
        "marker": "{}"
    }})",
        MPTID,
        MPTOUT2,
        ripple::strHex(GetAccountIDWithString(HOLDER1_ACCOUNT))
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = CreateMPTokenObject(HOLDER2_ACCOUNT, ripple::uint192(MPTID));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    auto const marker = GetAccountIDWithString(HOLDER1_ACCOUNT);
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, marker}));
    EXPECT_CALL(
        *backend, fetchMPTHolders(ripple::uint192(MPTID), testing::_, testing::Eq(marker), Const(30), testing::_)
    )
        .Times(1);

    auto const HOLDER1_ACCOUNTID = ripple::strHex(GetAccountIDWithString(HOLDER1_ACCOUNT));
    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}",
            "marker": "{}"
        }})",
        MPTID,
        HOLDER1_ACCOUNTID
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, MultipleMPTs)
{
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":50,
        "ledger_index": 30,
        "mptokens": [{}, {}],
        "validated": true
    }})",
        MPTID,
        MPTOUT1,
        MPTOUT2
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken1 = CreateMPTokenObject(HOLDER1_ACCOUNT, ripple::uint192(MPTID));
    auto const mptoken2 = CreateMPTokenObject(HOLDER2_ACCOUNT, ripple::uint192(MPTID));
    std::vector<Blob> const mpts = {mptoken1.getSerializer().peekData(), mptoken2.getSerializer().peekData()};
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, {}}));
    EXPECT_CALL(
        *backend, fetchMPTHolders(ripple::uint192(MPTID), testing::_, testing::Eq(std::nullopt), Const(30), testing::_)
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}"
        }})",
        MPTID
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, LimitMoreThanMAx)
{
    auto const currentOutput = fmt::format(
        R"({{
        "mpt_issuance_id": "{}",
        "limit":100,
        "ledger_index": 30,
        "mptokens": [{}],
        "validated": true
    }})",
        MPTID,
        MPTOUT1
    );

    backend->setRange(10, 30);
    auto ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = ripple::keylet::mptIssuance(ripple::uint192(MPTID)).key;
    ON_CALL(*backend, doFetchLedgerObject(issuanceKk, 30, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = CreateMPTokenObject(HOLDER1_ACCOUNT, ripple::uint192(MPTID));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend, fetchMPTHolders).WillByDefault(Return(MPTHoldersAndCursor{mpts, {}}));
    EXPECT_CALL(
        *backend,
        fetchMPTHolders(
            ripple::uint192(MPTID),
            Const(MPTHoldersHandler::LIMIT_MAX),
            testing::Eq(std::nullopt),
            Const(30),
            testing::_
        )
    )
        .Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "mpt_issuance_id": "{}",
            "limit": {}
        }})",
        MPTID,
        MPTHoldersHandler::LIMIT_MAX + 1
    ));
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(currentOutput), *output);
    });
}