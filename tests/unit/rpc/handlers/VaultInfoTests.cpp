#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/VaultInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"
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
constexpr auto kIndex1 = "ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890";
constexpr auto kSeq = 30;
constexpr auto kAssetCurrency = "XRP";
constexpr auto kAssetIssuer = "rrrrrrrrrrrrrrrrrrrrrhoLvTp";
constexpr auto kVaultId = "61B03A6F8CEBD3AF9D8F696C3D0A9A9F0493B34BF6B5D93CF0BC009E6BA75303";
constexpr auto kApiVersion = 2;

}  // namespace

struct RPCVaultInfoHandlerTest : HandlerBaseTest {
    RPCVaultInfoHandlerTest()
    {
        backend_->setRange(10, kSeq);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

struct VaultInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    CombinedError expectedErrorCode;
    std::string expectedErrorMessage;
};

struct VaultInfoParameterTest : RPCVaultInfoHandlerTest,
                                WithParamInterface<VaultInfoParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<VaultInfoParamTestCaseBundle>{
        VaultInfoParamTestCaseBundle{
            .testName = "RandomField",
            .testJson = R"JSON({
                "idk": "idk"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "MissingOwnerInVault",
            .testJson = R"JSON({
                "seq": 4
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "MissingSeqInVault",
            .testJson = R"JSON({
                "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "SeqNotAnInteger",
            .testJson = R"JSON({
                "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
                "seq": "asdf"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "OwnerNotAString",
            .testJson = R"JSON({
                "owner": true,
                "seq": 3
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "OwnerNotHexString"
        },
        VaultInfoParamTestCaseBundle{
            .testName = "OwnerNotAHexString",
            .testJson = R"JSON({
                "owner": "asdf",
                "seq": 3
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "OwnerNotHexString"
        },
        VaultInfoParamTestCaseBundle{
            .testName = "vaultIDNotString",
            .testJson = R"JSON({
                "vault_id": 3
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "vaultIDNotHex256",
            .testJson = R"JSON({
                "vault_id": "idk"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "vaultIDWithOwner",
            .testJson = fmt::format(
                R"JSON({{
                    "vault_id": "{}",
                    "owner": "{}"
                }})JSON",
                kVaultId,
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorCode = ClioError::RpcMalformedRequest,
            .expectedErrorMessage = "Malformed request."
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCVaultInfoGroup,
    VaultInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(VaultInfoParameterTest, InvalidParams)
{
    auto const testBundle = VaultInfoParameterTest::GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{VaultInfoHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(
            err.at("error_code").as_uint64(),
            std::visit(
                [](auto code) { return static_cast<uint32_t>(code); }, testBundle.expectedErrorCode
            )
        );
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCVaultInfoHandlerTest, InputHasOwnerButNotFoundResultsInError)
{
    auto const ledgerHeader = createLedgerHeader(kIndex1, kSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // Input JSON using vault object
    auto static const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "owner": "{}",
                "seq": 3
            }})JSON",
            kAccount
        )
    );

    // Run the handler
    auto const handler = AnyHandler{VaultInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output =
            handler.process(kInput, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
        EXPECT_EQ(err.at("error_code").as_uint64(), rpc::RippledError::RpcEntryNotFound);
        EXPECT_EQ(err.at("error_message").as_string(), "Entry not found.");
    });
}

TEST_F(RPCVaultInfoHandlerTest, VaultIDFailsVaultDeserializationReturnsEntryNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kIndex1, kSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // Mock: vault_id exists, but data is not a valid vault object
    xrpl::uint256 const vaultKey = xrpl::uint256{kVaultId};
    EXPECT_CALL(*backend_, doFetchLedgerObject(vaultKey, kSeq, _))
        .WillOnce(Return(std::nullopt));  // intentionally invalid vault

    auto const kInput = boost::json::parse(
        fmt::format(
            R"({{
            "vault_id": "{}"
        }})",
            kVaultId
        )
    );

    auto const handler = AnyHandler{VaultInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output =
            handler.process(kInput, Context{.yield = yield, .apiVersion = kApiVersion});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
        EXPECT_EQ(err.at("error_code").as_uint64(), rpc::RippledError::RpcEntryNotFound);
        EXPECT_EQ(err.at("error_message").as_string(), "vault object not found.");
    });
}

TEST_F(RPCVaultInfoHandlerTest, MissingIssuanceObject)
{
    auto const ledgerHeader = createLedgerHeader(kIndex1, kSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    xrpl::uint192 const mptSharesID{123};
    xrpl::uint256 const prevTxId{2};
    uint32_t const prevTxSeq = 3;
    uint64_t const ownerNode = 4;

    auto const vault = createVault(
        kAccount,
        kAccount2,
        kSeq,
        kAssetCurrency,
        kAssetIssuer,
        mptSharesID,
        ownerNode,
        prevTxId,
        prevTxSeq
    );

    auto const vaultKeylet = xrpl::keylet::vault(xrpl::uint256{kVaultId}).key;
    auto const mptIssuance = xrpl::keylet::mptIssuance(mptSharesID).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject(vaultKeylet, kSeq, _))
        .WillOnce(Return(vault.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(mptIssuance, kSeq, _))
        .WillOnce(Return(std::nullopt));  // Missing issuance

    auto static const kInput = boost::json::parse(
        fmt::format(
            R"({{
            "vault_id": "{}"
        }})",
            kVaultId
        )
    );

    auto const handler = AnyHandler{VaultInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output =
            handler.process(kInput, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
        EXPECT_EQ(err.at("error_code").as_uint64(), rpc::RippledError::RpcEntryNotFound);
        EXPECT_EQ(err.at("error_message").as_string(), "issuance object not found.");
    });
}

TEST_F(RPCVaultInfoHandlerTest, ValidVaultObjectQueryByVaultID)
{
    constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_index": 30,
            "validated": true,
            "vault": {
                "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Asset": {
                    "currency": "XRP"
                },
                "AssetsAvailable": "300",
                "AssetsTotal": "300",
                "Flags": 0,
                "LedgerEntryType": "Vault",
                "LossUnrealized": "1",
                "Owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "OwnerNode": "4",
                "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000002",
                "PreviousTxnLgrSeq": 3,
                "Sequence": 30,
                "ShareMPTID": "00000000000000000000000000000000000000000000007B",
                "WithdrawalPolicy": 200,
                "index": "61B03A6F8CEBD3AF9D8F696C3D0A9A9F0493B34BF6B5D93CF0BC009E6BA75303",
                "shares": {
                    "Flags": 0,
                    "Issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "LedgerEntryType": "MPTokenIssuance",
                    "MPTokenMetadata": "6D65746164617461",
                    "OutstandingAmount": "0",
                    "OwnerNode": "0",
                    "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                    "PreviousTxnLgrSeq": 0,
                    "Sequence": 30,
                    "index": "87658CA4D4D7A50EE99E632055FE7A879CD9A331880AC21D538FA6E4032804E3",
                    "mpt_issuance_id": "0000001E4B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                }
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kIndex1, kSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // Vault params
    xrpl::uint192 const mptSharesID{123};
    xrpl::uint256 const prevTxId{2};
    uint32_t const prevTxSeq = 3;
    uint64_t const ownerNode = 4;

    // Mock vault object
    auto const vault = createVault(
        kAccount,
        kAccount2,
        kSeq,
        kAssetCurrency,
        kAssetIssuer,
        mptSharesID,
        ownerNode,
        prevTxId,
        prevTxSeq
    );

    // Set up keylet based on vaultID
    auto const issuance = createMptIssuanceObject(kAccount, kSeq, "metadata");
    auto const vaultKeylet = xrpl::keylet::vault(xrpl::uint256{kVaultId}).key;
    auto const mptIssuance = xrpl::keylet::mptIssuance(mptSharesID).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject(vaultKeylet, kSeq, _))
        .WillOnce(Return(vault.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(mptIssuance, kSeq, _))
        .WillOnce(Return(issuance.getSerializer().peekData()));

    // Input JSON using vault_id
    auto static const kInput = boost::json::parse(
        fmt::format(
            R"({{
            "vault_id": "{}"
        }})",
            kVaultId
        )
    );

    // Run the handler
    auto const handler = AnyHandler{VaultInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output =
            handler.process(kInput, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCVaultInfoHandlerTest, ValidVaultObjectQueryByOwnerAndSeq)
{
    constexpr auto kExpectedOutput =
        R"JSON({
            "ledger_index": 30,
            "validated": true,
            "vault": {
                "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Asset": {
                    "currency": "XRP"
                },
                "AssetsAvailable": "300",
                "AssetsTotal": "300",
                "Flags": 0,
                "LedgerEntryType": "Vault",
                "LossUnrealized": "1",
                "Owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "OwnerNode": "4",
                "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000002",
                "PreviousTxnLgrSeq": 3,
                "Sequence": 30,
                "ShareMPTID": "00000000000000000000000000000000000000000000007B",
                "WithdrawalPolicy": 200,
                "index": "1B7BB49E0663E073D1C3EF989271F89E290AAF2D67CEE85F18E2CC76D168F694",
                "shares": {
                    "Flags": 0,
                    "Issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "LedgerEntryType": "MPTokenIssuance",
                    "MPTokenMetadata": "6D65746164617461",
                    "OutstandingAmount": "0",
                    "OwnerNode": "0",
                    "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                    "PreviousTxnLgrSeq": 0,
                    "Sequence": 30,
                    "index": "87658CA4D4D7A50EE99E632055FE7A879CD9A331880AC21D538FA6E4032804E3",
                    "mpt_issuance_id": "0000001E4B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                }
            }
        })JSON";

    auto const ledgerHeader = createLedgerHeader(kIndex1, kSeq);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(ledgerHeader));

    // Vault params
    xrpl::uint192 const mptSharesID{123};
    xrpl::uint256 const prevTxId{2};
    uint32_t const prevTxSeq = 3;
    uint64_t const ownerNode = 4;

    // Mock vault object
    auto const vault = createVault(
        kAccount,
        kAccount2,
        kSeq,
        kAssetCurrency,
        kAssetIssuer,
        mptSharesID,
        ownerNode,
        prevTxId,
        prevTxSeq
    );

    auto const issuance = createMptIssuanceObject(kAccount, kSeq, "metadata");

    auto const accountRoot = createAccountRootObject(kAccount, 0, kSeq, 200, 2, kIndex1, 2);
    auto const account = getAccountIdWithString(kAccount);
    auto const accountKeylet = xrpl::keylet::account(account).key;
    auto const vaultKeylet = xrpl::keylet::vault(account, kSeq).key;
    auto const mptIssuance = xrpl::keylet::mptIssuance(mptSharesID).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject(accountKeylet, kSeq, _))
        .WillOnce(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(vaultKeylet, kSeq, _))
        .WillOnce(Return(vault.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(mptIssuance, kSeq, _))
        .WillOnce(Return(issuance.getSerializer().peekData()));

    // Input JSON using vault object
    auto static const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "owner": "{}",
                "seq": {},
                "ledger_index": 30
            }})JSON",
            kAccount,
            kSeq
        )
    );

    // Run the handler
    auto const handler = AnyHandler{VaultInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output =
            handler.process(kInput, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kExpectedOutput));
    });
}
