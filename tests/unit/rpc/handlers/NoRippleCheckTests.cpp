#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NoRippleCheck.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/TxFlags.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kTxnId = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

}  // namespace

struct RPCNoRippleCheckTest : HandlerBaseTest {
    RPCNoRippleCheckTest()
    {
        backend_->setRange(10, 30);
    }
};

struct NoRippleParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct NoRippleCheckParameterTest : RPCNoRippleCheckTest,
                                    WithParamInterface<NoRippleParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<NoRippleParamTestCaseBundle>{
        NoRippleParamTestCaseBundle{
            .testName = "AccountNotExists",
            .testJson = R"JSON({
                "role": "gateway"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },
        NoRippleParamTestCaseBundle{
            .testName = "AccountNotString",
            .testJson = R"JSON({
                "account": 123,
                "role": "gateway"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },
        NoRippleParamTestCaseBundle{
            .testName = "InvalidAccount",
            .testJson = R"JSON({
                "account": "123",
                "role": "gateway"
             })JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        NoRippleParamTestCaseBundle{
            .testName = "InvalidRole",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "notrole"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "role field is invalid"
        },
        NoRippleParamTestCaseBundle{
            .testName = "RoleNotExists",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'role' missing"
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitNotInt",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": "gg"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitNegative",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": -1
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitZero",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": 0
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "TransactionsNotBool",
            .testJson = R"JSON({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "transactions": "gg"
             })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCNoRippleCheckGroup1,
    NoRippleCheckParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(NoRippleCheckParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(NoRippleCheckParameterTest, V1ApiTransactionsIsNotBool)
{
    static constexpr auto kReqJson = R"JSON(
        {
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "role": "gateway",
            "transactions": "gg"
         }
    )JSON";

    EXPECT_CALL(*backend_, fetchLedgerBySequence);
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const req = boost::json::parse(kReqJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::nullopt));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "role": "gateway",
                "ledger_hash": "{}"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaIntIndex)
{
    constexpr auto kSeq = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSeq, _)).WillByDefault(Return(std::nullopt));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "role": "gateway",
                "ledger_index": {}
            }})JSON",
            kAccount,
            kSeq
        )
    );
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaStringIndex)
{
    constexpr auto kSeq = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSeq, _)).WillByDefault(Return(std::nullopt));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "role": "gateway",
                "ledger_index": "{}"
            }})JSON",
            kAccount,
            kSeq
        )
    );
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kInput, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, AccountNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLedgerHash, 30);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return empty
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleSetTrustLineNoRippleSet)
{
    static constexpr auto kSeq = 30;
    static constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "problems": [
                "You appear to have set your default ripple flag even though you are not a gateway. This is not recommended unless you are experimenting"
            ],
            "validated": true
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(
            Return(createAccountRootObject(kAccount, xrpl::lsfDefaultRipple, 2, 200, 2, kIndex1, 2)
                       .getSerializer()
                       .peekData())
        );
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "user"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleUnsetTrustLineNoRippleUnSet)
{
    static constexpr auto kSeq = 30;
    static constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "problems": [
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "validated": true
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(
            createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "user"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleSetTrustLineNoRippleSet)
{
    static constexpr auto kSeq = 30;
    static constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "problems": [
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "validated": true
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(
            Return(createAccountRootObject(kAccount, xrpl::lsfDefaultRipple, 2, 200, 2, kIndex1, 2)
                       .getSerializer()
                       .peekData())
        );
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnset)
{
    static constexpr auto kSeq = 30;
    static constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "problems": [
                "You should immediately set your default ripple flag"
            ],
            "validated": true
        })JSON";

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(
            createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, 0
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway"
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(
    RPCNoRippleCheckTest,
    NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnsetHighAccount
)
{
    static constexpr auto kSeq = 30;

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(
            createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount2, 10, kAccount, 20, kTxnId, 123, 0
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount2, 10, kAccount, 20, kTxnId, 123, 0
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway",
                "transactions": true
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("transactions").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathLimit)
{
    constexpr auto kSeq = 30;

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(
            Return(createAccountRootObject(kAccount, xrpl::lsfDefaultRipple, 2, 200, 2, kIndex1, 2)
                       .getSerializer()
                       .peekData())
        );
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway",
                "limit": 1
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathTransactions)
{
    constexpr auto kSeq = 30;
    constexpr auto kTransactionSeq = 123;
    auto const expectedOutput = fmt::format(
        R"JSON({{
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "problems": [
                "You should immediately set your default ripple flag",
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "transactions": [
                {{
                    "Sequence": {},
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Fee": 1,
                    "TransactionType": "AccountSet",
                    "SetFlag": 8
                }},
                {{
                    "Sequence": {},
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Fee": 1,
                    "TransactionType": "TrustSet",
                    "LimitAmount": {{
                        "currency": "USD",
                        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "value": "10"
                    }},
                    "Flags": {}
                }},
                {{
                    "Sequence": {},
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "Fee": 1,
                    "TransactionType": "TrustSet",
                    "LimitAmount": {{
                        "currency": "USD",
                        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "value": "10"
                    }},
                    "Flags": {}
                }}
            ],
            "validated": true
        }})JSON",
        kTransactionSeq,
        kTransactionSeq + 1,
        xrpl::tfClearNoRipple,
        kTransactionSeq + 2,
        xrpl::tfClearNoRipple
    );

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(
            Return(createAccountRootObject(kAccount, 0, kTransactionSeq, 200, 2, kIndex1, 2)
                       .getSerializer()
                       .peekData())
        );
    auto const ownerDir =
        createOwnerDirLedgerObject({xrpl::uint256{kIndex1}, xrpl::uint256{kIndex2}}, kIndex1);
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway",
                "transactions": true
            }})JSON",
            kAccount,
            kLedgerHash
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, LimitMoreThanMax)
{
    constexpr auto kSeq = 30;

    auto ledgerHeader = createLedgerHeader(kLedgerHash, kSeq);
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(
            Return(createAccountRootObject(kAccount, xrpl::lsfDefaultRipple, 2, 200, 2, kIndex1, 2)
                       .getSerializer()
                       .peekData())
        );
    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{NoRippleCheckHandler::kLimitMax + 1, xrpl::uint256{kIndex1}}, kIndex1
    );
    auto const ownerDirKk = xrpl::keylet::ownerDir(getAccountIdWithString(kAccount)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSeq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kIssuer, 100, kAccount, 10, kAccount2, 20, kTxnId, 123, xrpl::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.reserve(NoRippleCheckHandler::kLimitMax + 1);
    for (auto i = 0; i < NoRippleCheckHandler::kLimitMax + 1; i++) {
        bbs.push_back(line1.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "account": "{}",
                "ledger_hash": "{}",
                "role": "gateway",
                "limit": {}
            }})JSON",
            kAccount,
            kLedgerHash,
            NoRippleCheckHandler::kLimitMax + 1
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result->as_object().at("problems").as_array().size(),
            NoRippleCheckHandler::kLimitMax
        );
    });
}
