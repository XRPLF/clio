#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/MPTHolders.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kHoldeR1Account = "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN";
constexpr auto kHoldeR2Account = "rEiNkzogdHEzUxPfsri5XSMqtXUixf2Yx";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405B0E3E";

std::string const kMptOuT1 =
    R"JSON({
        "account": "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN",
        "flags": 0,
        "mpt_amount": "1",
        "mptoken_index": "D137F2E5A5767A06CB7A8F060ADE442A30CFF95028E1AF4B8767E3A56877205A"
    })JSON";

std::string const kMptOuT2 =
    R"JSON({
        "account": "rEiNkzogdHEzUxPfsri5XSMqtXUixf2Yx",
        "flags": 0,
        "mpt_amount": "1",
        "mptoken_index": "36D91DEE5EFE4A93119A8B84C944A528F2B444329F3846E49FE921040DE17E65"
    })JSON";

}  // namespace

struct RPCMPTHoldersHandlerTest : HandlerBaseTest {
    RPCMPTHoldersHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCMPTHoldersHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_hash": "xxx"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_hash": 123
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "ledger_index": "notvalidated"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: issuer invalid format, length is incorrect
TEST_F(RPCMPTHoldersHandlerTest, MPTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(R"JSON({
            "mpt_issuance_id": "xxx"
        })JSON");
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "mpt_issuance_idMalformed");
    });
}

// error case: issuer missing
TEST_F(RPCMPTHoldersHandlerTest, MPTIDMissing)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(R"JSON({})JSON");
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Required field 'mpt_issuance_id' missing");
    });
}

// error case: issuer invalid format
TEST_F(RPCMPTHoldersHandlerTest, MPTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(R"JSON({
            "mpt_issuance_id": 12
        })JSON");
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "mpt_issuance_idNotString");
    });
}

// error case: invalid marker format
TEST_F(RPCMPTHoldersHandlerTest, MarkerInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "marker": "xxx"
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerMalformed");
    });
}

// error case: invalid marker type
TEST_F(RPCMPTHoldersHandlerTest, MarkerNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance_id": "{}",
                    "marker": 1
                }})JSON",
                kMptId
            )
        );
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kMptId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_index": "4"
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    // mock fetchLedgerBySequence return empty
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(std::optional<xrpl::LedgerHeader>{}));
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_index": 4
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerHash2)
{
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerinfo = createLedgerHeader(kLedgerHash, 31);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kMptId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCMPTHoldersHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_index": "31"
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{MPTHoldersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// normal case when MPT does not exist
TEST_F(RPCMPTHoldersHandlerTest, MPTNotFound)
{
    auto ledgerinfo = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kMptId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "objectNotFound");
    });
}

// normal case when mpt has one holder
TEST_F(RPCMPTHoldersHandlerTest, DefaultParameters)
{
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 50,
            "ledger_index": 30,
            "mptokens": [{}],
            "validated": true
        }})JSON",
        kMptId,
        kMptOuT1
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = createMpTokenObject(kHoldeR1Account, xrpl::uint192(kMptId));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = {}}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId), testing::_, testing::Eq(std::nullopt), Const(30), testing::_
        )
    )
        .Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}"
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, CustomAmounts)
{
    // it's not possible to have locked_amount to be greater than mpt_amount,
    // we are simply testing the response parsing of the api
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 50,
            "ledger_index": 30,
            "mptokens": [{{
                "account": "rrnAZCqMahreZrKMcZU3t2DZ6yUndT4ubN",
                "flags": 0,
                "mpt_amount": "0",
                "mptoken_index": "D137F2E5A5767A06CB7A8F060ADE442A30CFF95028E1AF4B8767E3A56877205A"
            }}],
            "validated": true
        }})JSON",
        kMptId
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = createMpTokenObject(kHoldeR1Account, xrpl::uint192(kMptId), 0);
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = {}}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId), testing::_, testing::Eq(std::nullopt), Const(30), testing::_
        )
    )
        .Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}"
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, SpecificLedgerIndex)
{
    auto const specificLedger = 20;
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 50,
            "ledger_index": {},
            "mptokens": [{}],
            "validated": true
        }})JSON",
        kMptId,
        specificLedger,
        kMptOuT1
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, specificLedger);
    ON_CALL(*backend_, fetchLedgerBySequence(specificLedger, _)).WillByDefault(Return(ledgerInfo));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, specificLedger, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = createMpTokenObject(kHoldeR1Account, xrpl::uint192(kMptId));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = {}}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId),
            testing::_,
            testing::Eq(std::nullopt),
            Const(specificLedger),
            testing::_
        )
    )
        .Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "ledger_index": {}
            }})JSON",
            kMptId,
            specificLedger
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, MarkerParameter)
{
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 50,
            "ledger_index": 30,
            "mptokens": [{}],
            "validated": true,
            "marker": "{}"
        }})JSON",
        kMptId,
        kMptOuT2,
        xrpl::strHex(getAccountIdWithString(kHoldeR1Account))
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = createMpTokenObject(kHoldeR2Account, xrpl::uint192(kMptId));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    auto const marker = getAccountIdWithString(kHoldeR1Account);
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = marker}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId), testing::_, testing::Eq(marker), Const(30), testing::_
        )
    )
        .Times(1);

    auto const holder1AccountId = xrpl::strHex(getAccountIdWithString(kHoldeR1Account));
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "marker": "{}"
            }})JSON",
            kMptId,
            holder1AccountId
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, MultipleMPTs)
{
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 50,
            "ledger_index": 30,
            "mptokens": [{}, {}],
            "validated": true
        }})JSON",
        kMptId,
        kMptOuT1,
        kMptOuT2
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken1 = createMpTokenObject(kHoldeR1Account, xrpl::uint192(kMptId));
    auto const mptoken2 = createMpTokenObject(kHoldeR2Account, xrpl::uint192(kMptId));
    std::vector<Blob> const mpts = {
        mptoken1.getSerializer().peekData(), mptoken2.getSerializer().peekData()
    };
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = {}}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId), testing::_, testing::Eq(std::nullopt), Const(30), testing::_
        )
    )
        .Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}"
            }})JSON",
            kMptId
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}

TEST_F(RPCMPTHoldersHandlerTest, LimitMoreThanMAx)
{
    auto const currentOutput = fmt::format(
        R"JSON({{
            "mpt_issuance_id": "{}",
            "limit": 100,
            "ledger_index": 30,
            "mptokens": [{}],
            "validated": true
        }})JSON",
        kMptId,
        kMptOuT1
    );

    auto ledgerInfo = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerInfo));
    auto const issuanceKk = xrpl::keylet::mptIssuance(xrpl::uint192(kMptId)).key;
    ON_CALL(*backend_, doFetchLedgerObject(issuanceKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const mptoken = createMpTokenObject(kHoldeR1Account, xrpl::uint192(kMptId));
    std::vector<Blob> const mpts = {mptoken.getSerializer().peekData()};
    ON_CALL(*backend_, fetchMPTHolders)
        .WillByDefault(Return(MPTHoldersAndCursor{.mptokens = mpts, .cursor = {}}));
    EXPECT_CALL(
        *backend_,
        fetchMPTHolders(
            xrpl::uint192(kMptId),
            Const(MPTHoldersHandler::kLimitMax),
            testing::Eq(std::nullopt),
            Const(30),
            testing::_
        )
    )
        .Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "mpt_issuance_id": "{}",
                "limit": {}
            }})JSON",
            kMptId,
            MPTHoldersHandler::kLimitMax + 1
        )
    );
    runSpawn([&, this](auto& yield) {
        auto handler = AnyHandler{MPTHoldersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(currentOutput), *output.result);
    });
}
