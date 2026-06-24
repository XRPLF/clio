#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountCurrencies.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTxnId = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

}  // namespace

struct RPCAccountCurrenciesHandlerTest : HandlerBaseTest {
    RPCAccountCurrenciesHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCAccountCurrenciesHandlerTest, AccountNotExist)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaStringSequence)
{
    constexpr auto kSeq = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(12, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": "{}"
            }})JSON",
            kAccount,
            kSeq
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, LedgerNonExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, DefaultParameter)
{
    static constexpr auto kOutput = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "receive_currencies": [
            "EUR",
            "JPY"
        ],
        "send_currencies": [
            "EUR",
            "USD"
        ]
    })JSON";

    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(30, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject(
        {xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
    );
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // Account can receive USD 10 from Account2 and send USD 20 to Account2, now
    // the balance is 100, Account can only send USD to Account2
    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );
    // Account2 can receive JPY 10 from Account and send JPY 20 to Account, now
    // the balance is 100, Account2 can only send JPY to Account
    auto const line2 = createRippleStateLedgerObject(
        "JPY", kIssuer, 100, kAccount2, 10, kAccount, 20, kTxnId, 123, 0
    );
    // Account can receive EUR 10 from Account and send EUR 20 to Account2, now
    // the balance is 8, Account can receive/send EUR to/from Account2
    auto const line3 = createRippleStateLedgerObject(
        "EUR", kIssuer, 8, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );
    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    bbs.push_back(line3.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}"
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOutput));
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderHash)
{
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex1}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCAccountCurrenciesHandlerTest, RequestViaLegderSeq)
{
    auto const ledgerSeq = 29;
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, ledgerSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(ledgerSeq, _)).WillByDefault(Return(ledgerHeader));
    // return valid account
    auto const accountKk = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, ledgerSeq, _))
        .WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = createOwnerDirLedgerObject({xrpl::uint256{kIndex1}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, ledgerSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_index": {}
            }})JSON",
            kAccount,
            ledgerSeq
        )
    );
    auto const handler = AnyHandler{AccountCurrenciesHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("ledger_index").as_uint64(), ledgerSeq);
    });
}

TEST(RPCAccountCurrenciesHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"account", "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59"},
        {"ledger_hash", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"ledger_index", 30},
        {"account_index", 1},
        {"strict", true}
    };
    auto const spec = AccountCurrenciesHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(
        warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated)
    );
    for (auto const& field : {"account_index", "strict"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)),
            std::string::npos
        );
    }
}
