#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <optional>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kNftId = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr auto kNftID2 = "00081388319F12E15BCA13E1B933BF4C99C8E1BBC36BD4910A85D52F00000022";

}  // namespace

struct RPCNFTInfoHandlerTest : HandlerBaseTest {
    RPCNFTInfoHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCNFTInfoHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "nft_id": "{}",
                    "ledger_hash": "xxx"
                }})JSON",
                kNftId
            )
        );
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCNFTInfoHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "nft_id": "{}",
                    "ledger_hash": 123
                }})JSON",
                kNftId
            )
        );
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCNFTInfoHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const input = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "nft_id": "{}",
                    "ledger_index": "notvalidated"
                }})JSON",
                kNftId
            )
        );
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: nft_id invalid format, length is incorrect
TEST_F(RPCNFTInfoHandlerTest, NFTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const input = boost::json::parse(R"JSON({
            "nft_id": "00080000B4F4AFC5FBCBD76873F18006173D2193467D3EE7"
        })JSON");
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idMalformed");
    });
}

// error case: nft_id invalid format
TEST_F(RPCNFTInfoHandlerTest, NFTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const input = boost::json::parse(R"JSON({
            "nft_id": 12
        })JSON");
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kNftId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence)
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_index": "4"
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence)
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_index": 4
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerHash2)
{
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 31);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kNftId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTInfoHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_index": "31"
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case nft does not exist
TEST_F(RPCNFTInfoHandlerTest, NonExistNFT)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch nft return empty
    ON_CALL(*backend_, fetchNFT).WillByDefault(Return(std::optional<NFT>{}));
    EXPECT_CALL(*backend_, fetchNFT(xrpl::uint256{kNftId}, 30, _)).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kNftId,
            kLedgerHash
        )
    );
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTInfoHandler{backend_}};
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "NFT not found");
    });
}

// normal case when only provide nft_id
TEST_F(RPCNFTInfoHandlerTest, DefaultParameters)
{
    static constexpr auto kCurrentOutput = R"JSON({
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
    })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(createNft(kNftId, kAccount, ledgerHeader.seq));
    ON_CALL(*backend_, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*backend_, fetchNFT(xrpl::uint256{kNftId}, 30, _)).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}"
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kCurrentOutput), *output.result);
    });
}

// nft is burned -> should not omit uri
TEST_F(RPCNFTInfoHandlerTest, BurnedNFT)
{
    static constexpr auto kCurrentOutput = R"JSON({
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
    })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(
        createNft(kNftId, kAccount, ledgerHeader.seq, xrpl::Blob{'u', 'r', 'i'}, true)
    );
    ON_CALL(*backend_, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*backend_, fetchNFT(xrpl::uint256{kNftId}, 30, _)).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}"
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kCurrentOutput), *output.result);
    });
}

// uri is not available -> should specify an empty string
TEST_F(RPCNFTInfoHandlerTest, NotBurnedNFTWithoutURI)
{
    static constexpr auto kCurrentOutput = R"JSON({
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
    })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft =
        std::make_optional<NFT>(createNft(kNftId, kAccount, ledgerHeader.seq, xrpl::Blob{}));
    ON_CALL(*backend_, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*backend_, fetchNFT(xrpl::uint256{kNftId}, 30, _)).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}"
            }})JSON",
            kNftId
        )
    );
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kCurrentOutput), *output.result);
    });
}

// check taxon field, transfer fee and serial
TEST_F(RPCNFTInfoHandlerTest, NFTWithExtraFieldsSet)
{
    static constexpr auto kCurrentOutput = R"JSON({
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
    })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // fetch nft return something
    auto const nft = std::make_optional<NFT>(createNft(kNftID2, kAccount, ledgerHeader.seq));
    ON_CALL(*backend_, fetchNFT).WillByDefault(Return(nft));
    EXPECT_CALL(*backend_, fetchNFT(xrpl::uint256{kNftID2}, 30, _)).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "nft_id": "{}"
            }})JSON",
            kNftID2
        )
    );
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTInfoHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kCurrentOutput), *output.result);
    });
}
