#include "data/Types.hpp"
#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerEntry.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kIndex1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kAccount3 = "rhzcyub9SbyZ4YF1JYskN5rLrTDUuLZG6D";
constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kTokenId = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr auto kNftId = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr auto kTxnId = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kCredentialType = "4B5943";

}  // namespace

struct RPCLedgerEntryTest : HandlerBaseTest {
    RPCLedgerEntryTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
    }
};

struct ParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerEntryParameterTest : public RPCLedgerEntryTest,
                                  public WithParamInterface<ParamTestCaseBundle> {};

// TODO: because we extract the error generation from the handler to framework
// the error messages need one round fine tuning
static auto
generateTestValuesForParametersTest()
{
    return std::vector<ParamTestCaseBundle>{
        ParamTestCaseBundle{
            .testName = "InvalidBinaryType",
            .testJson = R"JSON({
                "index":
                "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
                "binary": "invalid"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAccountRootFormat",
            .testJson = R"JSON({
                "account_root": "invalid"
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDidFormat",
            .testJson = R"JSON({
                "did": "invalid"
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAccountRootNotString",
            .testJson = R"JSON({
                "account_root": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "account_rootNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidLedgerIndex",
            .testJson = R"JSON({
                "ledger_index": "wrong"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },

        ParamTestCaseBundle{
            .testName = "UnknownOption",
            .testJson = R"JSON({})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "No ledger_entry params provided."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthType",
            .testJson = R"JSON({
                "deposit_preauth": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthString",
            .testJson = R"JSON({
                "deposit_preauth": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthEmptyJson",
            .testJson = R"JSON({
                "deposit_preauth": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonWrongAccount",
            .testJson = R"JSON({
                "deposit_preauth": {
                    "owner": "invalid",
                    "authorized": "invalid"
                }
            })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonOwnerNotString",
            .testJson = R"JSON({
                "deposit_preauth": {
                    "owner": 123,
                    "authorized": 123
                }
            })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizedNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": 123
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "authorizedNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizeCredentialsNotArray",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": "asdf"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "authorized_credentials not array"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizeCredentialsMalformedString",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": ["C2F2A19C8D0D893D18F18FDCFE13A3ECB41767E48422DF07F2455CDA08FDF09B"]
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "authorized_credentials elements in array are not objects."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthBothAuthAndAuthCredentialsDoesNotExists",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Must have one of authorized or authorized_credentials."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthBothAuthAndAuthCredentialsExists",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": "{}",
                        "authorized_credentials": [
                           {{
                                "issuer": "{}",
                                "credential_type": "{}"
                            }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kAccount3,
                kCredentialType
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Must have one of authorized or authorized_credentials."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthEmptyAuthorizeCredentials",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        ]
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Requires at least one element in authorized_credentials array."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsMissingCredentialType",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                            {{
                                "issuer": "{}"
                            }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Field 'CredentialType' is required but missing."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsMissingIssuer",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "credential_type": "{}"
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kCredentialType
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Field 'Issuer' is required but missing."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsIncorrectIssuerType",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": 123,
                            "credential_type": "{}"
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kCredentialType
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "issuer NotString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsIncorrectCredentialType",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": 432
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type NotString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsCredentialTypeNotHex",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": "hello world"
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type NotHexString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsCredentialTypeEmpty",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": ""
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type is empty"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthDuplicateAuthorizeCredentials",
            .testJson = fmt::format(
                R"JSON({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": "{}"
                        }},
                        {{
                            "issuer": "{}",
                            "credential_type": "{}"
                        }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kCredentialType,
                kAccount2,
                kCredentialType
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "duplicates in credentials."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketType",
            .testJson = R"JSON({
                "ticket": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketIndex",
            .testJson = R"JSON({
                "ticket": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketEmptyJson",
            .testJson = R"JSON({
                "ticket": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonAccountNotString",
            .testJson = R"JSON({
                "ticket": {
                    "account": 123,
                    "ticket_seq": 123
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonAccountInvalid",
            .testJson = R"JSON({
                "ticket": {
                    "account": "123",
                    "ticket_seq": 123
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonSeqNotInt",
            .testJson = fmt::format(
                R"JSON({{
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": "123"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferType",
            .testJson = R"JSON({
                "offer": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferIndex",
            .testJson = R"JSON({
                "offer": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferEmptyJson",
            .testJson = R"JSON({
                "offer": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonAccountNotString",
            .testJson = R"JSON({
                "ticket": {
                    "account": 123,
                    "seq": 123
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonAccountInvalid",
            .testJson = R"JSON({
                "ticket": {
                    "account": "123",
                    "seq": 123
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonSeqNotInt",
            .testJson = fmt::format(
                R"JSON({{
                    "offer": {{
                        "account": "{}",
                        "seq": "123"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowType",
            .testJson = R"JSON({
                "escrow": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowIndex",
            .testJson = R"JSON({
                "escrow": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowEmptyJson",
            .testJson = R"JSON({
                "escrow": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonAccountNotString",
            .testJson = R"JSON({
                "escrow": {
                    "owner": 123,
                    "seq": 123
                }
            })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonAccountInvalid",
            .testJson = R"JSON({
                "escrow": {
                    "owner": "123",
                    "seq": 123
                }
            })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonSeqNotInt",
            .testJson = fmt::format(
                R"JSON({{
                    "escrow": {{
                        "owner": "{}",
                        "seq": "123"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateType",
            .testJson = R"JSON({
                "ripple_state": "123"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateMissField",
            .testJson = R"JSON({
                "ripple_state": {
                    "currency": "USD"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateEmptyJson",
            .testJson = R"JSON({
                "ripple_state": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateOneAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}"]
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateSameAccounts",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}", "{}"],
                        "currency": "USD"
                    }}
                }})JSON",
                kAccount,
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongAccountsNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}",123],
                        "currency": "USD"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongAccountsFormat",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}", "123"],
                        "currency": "USD"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "malformedAddresses"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongCurrency",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}", "{}"],
                        "currency": "XXXX"
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedCurrency",
            .expectedErrorMessage = "malformedCurrency"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongCurrencyNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "ripple_state": {{
                        "accounts": ["{}", "{}"],
                        "currency": 123
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "currencyNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryType",
            .testJson = R"JSON({
                "directory": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryIndex",
            .testJson = R"JSON({
                "directory": "123"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryEmptyJson",
            .testJson = R"JSON({
                "directory": {}
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "missingOwnerOrDirRoot"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongOwnerNotString",
            .testJson = R"JSON({
                "directory": {
                    "owner": 123
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ownerNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongOwnerFormat",
            .testJson = R"JSON({
                "directory": {
                    "owner": "123"
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongDirFormat",
            .testJson = R"JSON({
                "directory": {
                    "dir_root": "123"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "dir_rootMalformed"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongDirNotString",
            .testJson = R"JSON({
                "directory": {
                    "dir_root": 123
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "dir_rootNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryDirOwnerConflict",
            .testJson = fmt::format(
                R"JSON({{
                    "directory": {{
                        "dir_root": "{}",
                        "owner": "{}"
                    }}
                }})JSON",
                kIndex1,
                kAccount
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "mayNotSpecifyBothDirRootAndOwner"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryDirSubIndexNotInt",
            .testJson = fmt::format(
                R"JSON({{
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": "not int"
                    }}
                }})JSON",
                kIndex1
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAMMStringIndex",
            .testJson = R"JSON({
                "amm": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMJson",
            .testJson = R"JSON({
                "amm": {}
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "NonObjectAMMJsonAsset",
            .testJson = R"JSON({
                "amm": {
                    "asset": 123,
                    "asset2": 123
                }
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMAssetJson",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset": {{}},
                        "asset2": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMAsset2Json",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{}},
                        "asset": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAsset2Json",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetJson",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAssetNotJson",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset": "invalid",
                        "asset2": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAsset2NotJson",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": "invalid",
                        "asset": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "WrongAMMAssetCurrency",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "XRP"
                        }},
                        "asset": {{
                            "currency": "USD2",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "WrongAMMAssetIssuer",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "XRP"
                        }},
                        "asset": {{
                            "currency": "USD",
                            "issuer": "aa{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetIssuerForNonXRP",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "JPY"
                        }},
                        "asset": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAssetHasIssuerForXRP",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "XRP",
                            "issuer": "{}"
                        }},
                        "asset": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetCurrency",
            .testJson = fmt::format(
                R"JSON({{
                    "amm": {{
                        "asset2": {{
                            "currency": "XRP"
                        }},
                        "asset": {{
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingBridgeAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeCurrencyIsNumber",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": {},
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount,
                1,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuerIsNumber",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": {}
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount,
                "JPY",
                2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuingChainIssueIsNotObject",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": 1
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeWithInvalidBridgeAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "abcd",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeDoorInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "abcd",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuerInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "invalid"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueCurrencyInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPJPJP",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount2,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueXRPCurrencyInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP",
                            "issuer": "{}"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount2,
                kAccount2,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueJPYCurrencyInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingLockingChainDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP",
                            "issuer": "{}"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kAccount2,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingIssuingChainDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingLockingChainIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "IssuingChainDoor": "{}",
                        "LockingChainDoor": "{}",
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingIssuingChainIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": {{
                        "IssuingChainDoor": "{}",
                        "LockingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeInvalidType",
            .testJson = fmt::format(
                R"JSON({{
                    "bridge_account": "{}",
                    "bridge": "invalid"
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdInvalidType",
            .testJson = R"JSON({
                "xchain_owned_claim_id": 123
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingClaimId",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_claim_id": {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_claim_id": {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount
            ),

            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonInvalidDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_claim_id": {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "abcd",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonInvalidIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_claim_id": {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdInvalidType",
            .testJson = R"JSON({
                "xchain_owned_create_account_claim_id": 123
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingClaimId",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_create_account_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_create_account_claim_id": {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_create_account_claim_id": {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount
            ),

            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonInvalidDoor",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_create_account_claim_id": {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "abcd",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                "JPY",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonInvalidIssue",
            .testJson = fmt::format(
                R"JSON({{
                    "xchain_owned_create_account_claim_id": {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdMissing",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidNegative",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": -1
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeString",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": "invalid"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeDouble",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 3.21
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeObject",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": {{}}
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeArray",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": []
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeNull",
            .testJson = fmt::format(
                R"JSON({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": null
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountMissing",
            .testJson = R"JSON({
                "oracle": {
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeInteger",
            .testJson = R"JSON({
                "oracle": {
                    "account": 123,
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeDouble",
            .testJson = R"JSON({
                "oracle": {
                    "account": 123.45,
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeNull",
            .testJson = R"JSON({
                "oracle": {
                    "account": null,
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeObject",
            .testJson = R"JSON({
                "oracle": {
                    "account": {"test": "test"},
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeArray",
            .testJson = R"JSON({
                "oracle": {
                    "account": [{"test": "test"}],
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidFormat",
            .testJson = R"JSON({
                "oracle": {
                    "account": "NotHex",
                    "oracle_document_id": 1
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidFormat",
            .testJson = R"JSON({
                "oracle": "NotHex"
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeInteger",
            .testJson = R"JSON({
                "oracle": 123
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeDouble",
            .testJson = R"JSON({
                "oracle": 123.45
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeArray",
            .testJson = R"JSON({
                "oracle": [{"test": "test"}]
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeNull",
            .testJson = R"JSON({
                "oracle": null
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidSubjectType",
            .testJson = R"JSON({
                "credential": {
                    "subject": 123
                }
            })JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidIssuerType",
            .testJson = fmt::format(
                R"JSON({{
                    "credential": {{
                        "issuer": ["{}"]
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTIssuanceStringIndex",
            .testJson = R"JSON({
                "mpt_issuance": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTIssuanceType",
            .testJson = R"JSON({
                "mpt_issuance": 0
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenStringIndex",
            .testJson = R"JSON({
                "mptoken": "invalid"
            })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenObject",
            .testJson = fmt::format(
                R"JSON({{
                    "mptoken": {{}}
                }})JSON"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "MissingMPTokenID",
            .testJson = fmt::format(
                R"JSON({{
                    "mptoken": {{
                        "account": "{}"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidCredentialType",
            .testJson = fmt::format(
                R"JSON({{
                    "credential": {{
                        "subject": "{}",
                        "issuer": "{}",
                        "credential_type": 1234
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialMissingIssuerField",
            .testJson = fmt::format(
                R"JSON({{
                    "credential": {{
                        "subject": "{}",
                        "credential_type": "1234"
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "mptoken": {{
                        "mpt_issuance_id": "0000019315EABA24E6135A4B5CE2899E0DA791206413B33D",
                        "account": 1
                    }}
                }})JSON"
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenType",
            .testJson = fmt::format(
                R"JSON({{
                    "mptoken": 0
                }})JSON"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_NotObject",
            .testJson = R"JSON({"permissioned_domain": []})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_InvalidString",
            .testJson = R"JSON({"permissioned_domain": "invalid_string"})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_EmptyObject",
            .testJson = R"JSON({"permissioned_domain": {}})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_BadAccount",
            .testJson = R"JSON({"permissioned_domain": {"account": "1234", "seq": 1234}})JSON",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_MissingSeq",
            .testJson = fmt::format(
                R"JSON({{
                    "permissioned_domain": {{ "account": "{}" }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_SeqIsNotUint",
            .testJson = fmt::format(
                R"JSON({{
                    "permissioned_domain": {{ "account": "{}", "seq": -1 }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidPermissionedDomain_BothAccountAndSeqAreInvalid",
            .testJson =
                R"JSON({
                    "permissioned_domain": { "account": "", "seq": -1 }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidVault_Type",
            .testJson =
                R"JSON({
                    "vault": 0
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidVault_NotHex",
            .testJson =
                R"JSON({
                    "vault": "invalid_hex"
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "MissingOwner",
            .testJson =
                R"JSON({
                    "vault": { "seq": 1 }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },

        ParamTestCaseBundle{
            .testName = "MissingSeq",
            .testJson =
                R"JSON({
                    "vault": { "owner": "abcd" }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "SeqNotInteger",
            .testJson =
                R"JSON({
                    "vault": {
                       "owner": "abcd",
                       "seq": "notAnInteger"
                    }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "InvalidOwnerFormat",
            .testJson =
                R"JSON({
                    "vault": {
                        "owner": "abcd",
                        "seq": 10
                    }
                })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner.",
        },
        ParamTestCaseBundle{
            .testName = "BothOwnerAndSeqInvalid",
            .testJson =
                R"JSON({
                    "vault": {
                        "owner": "abcd",
                        "seq": -200
                    }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        // LoanBroker tests
        ParamTestCaseBundle{
            .testName = "LoanBroker_InvalidType",
            .testJson = R"JSON({"loan_broker": 0})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_NotHex",
            .testJson =
                R"JSON({
                    "loan_broker": "invalid_hex"
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_MissingOwner",
            .testJson =
                R"JSON({
                    "loan_broker": { "seq": 1 }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_MissingSeq",
            .testJson =
                R"JSON({
                    "loan_broker": { "owner": "abcd" }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_SeqNotInteger",
            .testJson = fmt::format(
                R"JSON({{
                    "loan_broker": {{
                       "owner": "{}",
                       "seq": "notAnInteger"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_InvalidOwnerFormat",
            .testJson =
                R"JSON({
                    "loan_broker": {
                        "owner": "abcd",
                        "seq": 10
                    }
                })JSON",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner.",
        },
        ParamTestCaseBundle{
            .testName = "LoanBroker_NegativeSeq",
            .testJson =
                R"JSON({
                    "loan_broker": {
                        "owner": "abcd",
                        "seq": -200
                    }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        // Loan tests
        ParamTestCaseBundle{
            .testName = "Loan_InvalidType",
            .testJson = R"JSON({"loan": 0})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_NotHex",
            .testJson =
                R"JSON({
                    "loan": "invalid_hex"
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_MissingLoanBrokerId",
            .testJson =
                R"JSON({
                    "loan": { "loan_seq": 1 }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_MissingLoanSeq",
            .testJson = fmt::format(
                R"JSON({{
                    "loan": {{ "loan_broker_id": "{}" }}
                }})JSON",
                kIndex1
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_LoanSeqNotInteger",
            .testJson = fmt::format(
                R"JSON({{
                    "loan": {{
                       "loan_broker_id": "{}",
                       "loan_seq": "notAnInteger"
                    }}
                }})JSON",
                kIndex1
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_InvalidLoanBrokerIdFormat",
            .testJson =
                R"JSON({
                    "loan": {
                        "loan_broker_id": "invalid_hex",
                        "loan_seq": 10
                    }
                })JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Loan_NegativeLoanSeq",
            .testJson = fmt::format(
                R"JSON({{
                    "loan": {{
                        "loan_broker_id": "{}",
                        "loan_seq": -200
                    }}
                }})JSON",
                kIndex1
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request.",
        },
        ParamTestCaseBundle{
            .testName = "Delegate_InvalidType",
            .testJson = R"JSON({"delegate": 123})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_InvalidStringIndex",
            .testJson = R"JSON({"delegate": "invalid_hex_string"})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_EmptyObject",
            .testJson = R"JSON({"delegate": {}})JSON",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_MissingAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "authorize": "{}"
                    }}
                }})JSON",
                kAccount2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_AccountNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "account": 123,
                        "authorize": "{}"
                    }}
                }})JSON",
                kAccount2
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_AccountInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "account": "invalid_address",
                        "authorize": "{}"
                    }}
                }})JSON",
                kAccount2
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_MissingAuthorize",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "account": "{}"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_AuthorizeNotString",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "account": "{}",
                        "authorize": 123
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "Delegate_AuthorizeInvalid",
            .testJson = fmt::format(
                R"JSON({{
                    "delegate": {{
                        "account": "{}",
                        "authorize": "invalid_address"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup1,
    LedgerEntryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(LedgerEntryParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

// parameterized test cases for index
struct IndexTest : public HandlerBaseTest, public WithParamInterface<std::string> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            return static_cast<std::string>(info.param);
        }
    };
};

// content of index, amendments, check, fee, hashes, nft_offer, nunl, nft_page, payment_channel,
// signer_list fields is ledger index.
INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup3,
    IndexTest,
    Values(
        "index",
        "amendments",
        "check",
        "fee",
        "hashes",
        "nft_offer",
        "nunl",
        "nft_page",
        "payment_channel",
        "signer_list"
    ),
    IndexTest::NameGenerator{}
);

TEST_P(IndexTest, InvalidIndexUint256)
{
    auto const index = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "{}": "invalid"
                }})JSON",
                index
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "malformedRequest");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed request.");
    });
}

TEST_P(IndexTest, InvalidIndexNotString)
{
    auto const index = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "{}": 123
                }})JSON",
                index
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "malformedRequest");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed request.");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerEntryNotFound)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    // return null for ledger entry
    auto const key = xrpl::keylet::account(getAccountIdWithString(kAccount)).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(key, kRangeMax, _))
        .WillRepeatedly(Return(std::optional<Blob>{}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "account_root": "{}"
                }})JSON",
                kAccount
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
    });
}

struct NormalPathTestBundle {
    std::string testName;
    std::string testJson;
    xrpl::uint256 expectedIndex;
    xrpl::STObject mockedEntity;
};

struct RPCLedgerEntryNormalPathTest : public RPCLedgerEntryTest,
                                      public WithParamInterface<NormalPathTestBundle> {};

static auto
generateTestValuesForNormalPathTest()
{
    auto account1 = getAccountIdWithString(kAccount);
    auto account2 = getAccountIdWithString(kAccount2);
    xrpl::Currency currency;
    xrpl::toCurrency(currency, "USD");

    return std::vector<NormalPathTestBundle>{
        NormalPathTestBundle{
            .testName = "Index",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "index": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity =
                createAccountRootObject(kAccount2, xrpl::lsfGlobalFreeze, 1, 10, 2, kIndex1, 3)
        },
        NormalPathTestBundle{
            .testName = "Payment_channel",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "payment_channel": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity =
                createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 200, 300, kIndex1, 400)
        },
        NormalPathTestBundle{
            .testName = "Nft_page",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "nft_page": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createNftTokenPage(
                std::vector{std::make_pair<std::string, std::string>(kTokenId, "www.ok.com")},
                std::nullopt
            )
        },
        NormalPathTestBundle{
            .testName = "Check",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "check": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createCheckLedgerObject(kAccount, kAccount2)
        },
        NormalPathTestBundle{
            .testName = "DirectoryIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "directory": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createOwnerDirLedgerObject(
                std::vector<xrpl::uint256>{xrpl::uint256{kIndex1}}, kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "OfferIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "offer": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createOfferLedgerObject(
                kAccount,
                100,
                200,
                "USD",
                "XRP",
                kAccount2,
                xrpl::toBase58(xrpl::xrpAccount()),
                kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "EscrowIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "escrow": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createEscrowLedgerObject(kAccount, kAccount2)
        },
        NormalPathTestBundle{
            .testName = "TicketIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "ticket": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createTicketLedgerObject(kAccount, 0)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "deposit_preauth": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createDepositPreauthLedgerObjectByAuth(kAccount, kAccount2)
        },
        NormalPathTestBundle{
            .testName = "AccountRoot",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "account_root": "{}"
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::account(getAccountIdWithString(kAccount)).key,
            .mockedEntity = createAccountRootObject(kAccount, 0, 1, 1, 1, kIndex1, 1)
        },
        NormalPathTestBundle{
            .testName = "DID",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "did": "{}"
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::did(getAccountIdWithString(kAccount)).key,
            .mockedEntity = createDidObject(kAccount, "mydocument", "myURI", "mydata")
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaDirRoot",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": 2
                    }}
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::keylet::page(xrpl::uint256{kIndex1}, 2).key,
            .mockedEntity = createOwnerDirLedgerObject(
                std::vector<xrpl::uint256>{xrpl::uint256{kIndex1}}, kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaOwner",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}",
                        "sub_index": 2
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::page(xrpl::keylet::ownerDir(account1), 2).key,
            .mockedEntity = createOwnerDirLedgerObject(
                std::vector<xrpl::uint256>{xrpl::uint256{kIndex1}}, kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaDefaultSubIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}"
                    }}
                }})JSON",
                kAccount
            ),
            // default sub_index is 0
            .expectedIndex = xrpl::keylet::page(xrpl::keylet::ownerDir(account1), 0).key,
            .mockedEntity = createOwnerDirLedgerObject(
                std::vector<xrpl::uint256>{xrpl::uint256{kIndex1}}, kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "Escrow",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "escrow": {{
                        "owner": "{}",
                        "seq": 1
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::escrow(account1, 1).key,
            .mockedEntity = createEscrowLedgerObject(kAccount, kAccount2)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthByAuth",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": "{}"
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedIndex = xrpl::keylet::depositPreauth(account1, account2).key,
            .mockedEntity = createDepositPreauthLedgerObjectByAuth(kAccount, kAccount2)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthByAuthCredentials",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                            {{
                                 "issuer": "{}",
                                 "credential_type": "{}"
                            }}
                        ]
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kCredentialType
            ),
            .expectedIndex = xrpl::keylet::depositPreauth(
                                 account1,
                                 credentials::createAuthCredentials(createAuthCredentialArray(
                                     std::vector<std::string_view>{kAccount2},
                                     std::vector<std::string_view>{kCredentialType}
                                 ))
            )
                                 .key,
            .mockedEntity = createDepositPreauthLedgerObjectByAuthCredentials(
                kAccount, kAccount2, kCredentialType
            )
        },
        NormalPathTestBundle{
            .testName = "Credentials",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "credential": {{
                        "subject": "{}",
                        "issuer": "{}",
                        "credential_type": "{}"
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kCredentialType
            ),
            .expectedIndex = xrpl::keylet::credential(
                                 account1,
                                 account2,
                                 xrpl::Slice(
                                     // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                     xrpl::strUnHex(kCredentialType)->data(),
                                     // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                     xrpl::strUnHex(kCredentialType)->size()
                                 )
            )
                                 .key,
            .mockedEntity = createCredentialObject(kAccount, kAccount2, kCredentialType)
        },
        NormalPathTestBundle{
            .testName = "RippleState",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "ripple_state": {{
                        "accounts": ["{}", "{}"],
                        "currency": "USD"
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedIndex = xrpl::keylet::line(account1, account2, currency).key,
            .mockedEntity = createRippleStateLedgerObject(
                "USD", kAccount2, 100, kAccount, 10, kAccount2, 20, kIndex1, 123, 0
            )
        },
        NormalPathTestBundle{
            .testName = "Ticket",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": 2
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::getTicketIndex(account1, 2),
            .mockedEntity = createTicketLedgerObject(kAccount, 0)
        },
        NormalPathTestBundle{
            .testName = "Offer",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "offer": {{
                        "account": "{}",
                        "seq": 2
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::offer(account1, 2).key,
            .mockedEntity = createOfferLedgerObject(
                kAccount,
                100,
                200,
                "USD",
                "XRP",
                kAccount2,
                xrpl::toBase58(xrpl::xrpAccount()),
                kIndex1
            )
        },
        NormalPathTestBundle{
            .testName = "AMMViaIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "amm": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createAmmObject(
                kAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "JPY", kAccount2
            )
        },
        NormalPathTestBundle{
            .testName = "AMMViaJson",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "amm": {{
                        "asset": {{
                            "currency": "XRP"
                        }},
                        "asset2": {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                "JPY",
                kAccount2
            ),
            .expectedIndex =
                xrpl::keylet::amm(
                    getIssue("XRP", xrpl::toBase58(xrpl::xrpAccount())), getIssue("JPY", kAccount2)
                )
                    .key,
            .mockedEntity = createAmmObject(
                kAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), "JPY", kAccount2
            )
        },
        NormalPathTestBundle{
            .testName = "BridgeLocking",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount,
                kAccount,
                kAccount2,
                kAccount3
            ),
            .expectedIndex = xrpl::keylet::bridge(
                                 xrpl::STXChainBridge(
                                     getAccountIdWithString(kAccount),
                                     xrpl::xrpIssue(),
                                     getAccountIdWithString(kAccount2),
                                     getIssue("JPY", kAccount3)
                                 ),
                                 xrpl::STXChainBridge::ChainType::Locking
            )
                                 .key,
            .mockedEntity = createBridgeObject(kAccount, kAccount, kAccount2, "JPY", kAccount3)
        },
        NormalPathTestBundle{
            .testName = "BridgeIssuing",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})JSON",
                kAccount2,
                kAccount,
                kAccount2,
                kAccount3
            ),
            .expectedIndex = xrpl::keylet::bridge(
                                 xrpl::STXChainBridge(
                                     getAccountIdWithString(kAccount),
                                     xrpl::xrpIssue(),
                                     getAccountIdWithString(kAccount2),
                                     getIssue("JPY", kAccount3)
                                 ),
                                 xrpl::STXChainBridge::ChainType::Issuing
            )
                                 .key,
            .mockedEntity = createBridgeObject(kAccount, kAccount, kAccount2, "JPY", kAccount3)
        },
        NormalPathTestBundle{
            .testName = "XChainOwnedClaimId",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "xchain_owned_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }},
                        "xchain_owned_claim_id": 10
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kAccount3
            ),
            .expectedIndex = xrpl::keylet::xChainClaimID(
                                 xrpl::STXChainBridge(
                                     getAccountIdWithString(kAccount),
                                     xrpl::xrpIssue(),
                                     getAccountIdWithString(kAccount2),
                                     getIssue("JPY", kAccount3)
                                 ),
                                 10
            )
                                 .key,
            .mockedEntity = createChainOwnedClaimIdObject(
                kAccount, kAccount, kAccount2, "JPY", kAccount3, kAccount
            )
        },
        NormalPathTestBundle{
            .testName = "XChainOwnedCreateAccountClaimId",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "xchain_owned_create_account_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }},
                        "xchain_owned_create_account_claim_id": 10
                    }}
                }})JSON",
                kAccount,
                kAccount2,
                kAccount3
            ),
            .expectedIndex = xrpl::keylet::xChainCreateAccountClaimID(
                                 xrpl::STXChainBridge(
                                     getAccountIdWithString(kAccount),
                                     xrpl::xrpIssue(),
                                     getAccountIdWithString(kAccount2),
                                     getIssue("JPY", kAccount3)
                                 ),
                                 10
            )
                                 .key,
            .mockedEntity = createChainOwnedClaimIdObject(
                kAccount, kAccount, kAccount2, "JPY", kAccount3, kAccount
            )
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaIntOracleDocumentId",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 1
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::oracle(getAccountIdWithString(kAccount), 1).key,
            .mockedEntity = createOracleObject(
                kAccount,
                "70726F7669646572",
                32u,
                1234u,
                xrpl::Blob(8, 's'),
                xrpl::Blob(8, 's'),
                kRangeMax - 2,
                xrpl::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                createPriceDataSeries({createOraclePriceData(
                    2e4, xrpl::toCurrency("XRP"), xrpl::toCurrency("USD"), 3
                )})
            )
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaStrOracleDocumentId",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": "1"
                    }}
                }})JSON",
                kAccount
            ),
            .expectedIndex = xrpl::keylet::oracle(getAccountIdWithString(kAccount), 1).key,
            .mockedEntity = createOracleObject(
                kAccount,
                "70726F7669646572",
                32u,
                1234u,
                xrpl::Blob(8, 's'),
                xrpl::Blob(8, 's'),
                kRangeMax - 2,
                xrpl::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                createPriceDataSeries({createOraclePriceData(
                    2e4, xrpl::toCurrency("XRP"), xrpl::toCurrency("USD"), 3
                )})
            )
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaString",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "oracle": "{}"
                }})JSON",
                xrpl::to_string(xrpl::keylet::oracle(getAccountIdWithString(kAccount), 1).key)
            ),
            .expectedIndex = xrpl::keylet::oracle(getAccountIdWithString(kAccount), 1).key,
            .mockedEntity = createOracleObject(
                kAccount,
                "70726F7669646572",
                64u,
                4321u,
                xrpl::Blob(8, 'a'),
                xrpl::Blob(8, 'a'),
                kRangeMax - 4,
                xrpl::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                createPriceDataSeries({createOraclePriceData(
                    1e3, xrpl::toCurrency("USD"), xrpl::toCurrency("XRP"), 2
                )})
            )
        },
        NormalPathTestBundle{
            .testName = "MPTIssuance",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "mpt_issuance": "{}"
                }})JSON",
                xrpl::to_string(xrpl::makeMptID(2, account1))
            ),
            .expectedIndex = xrpl::keylet::mptIssuance(xrpl::makeMptID(2, account1)).key,
            .mockedEntity = createMptIssuanceObject(kAccount, 2, "metadata")
        },
        NormalPathTestBundle{
            .testName = "MPTokenViaIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "mptoken": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity = createMpTokenObject(kAccount, xrpl::makeMptID(2, account1))
        },
        NormalPathTestBundle{
            .testName = "MPTokenViaObject",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "mptoken": {{
                        "account": "{}",
                        "mpt_issuance_id": "{}"
                    }}
                }})JSON",
                kAccount,
                xrpl::to_string(xrpl::makeMptID(2, account1))
            ),
            .expectedIndex = xrpl::keylet::mptoken(xrpl::makeMptID(2, account1), account1).key,
            .mockedEntity = createMpTokenObject(kAccount, xrpl::makeMptID(2, account1))
        },
        NormalPathTestBundle{
            .testName = "PermissionedDomainViaString",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "permissioned_domain": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256(kIndex1),
            .mockedEntity =
                createPermissionedDomainObject(kAccount, kIndex1, kRangeMax, 0, xrpl::uint256{0}, 0)
        },
        NormalPathTestBundle{
            .testName = "PermissionedDomainViaObject",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "permissioned_domain": {{
                        "account": "{}",
                        "seq": {}
                    }}
                }})JSON",
                kAccount,
                kRangeMax
            ),
            .expectedIndex = xrpl::keylet::permissionedDomain(
                                 // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                 *xrpl::parseBase58<xrpl::AccountID>(kAccount),
                                 kRangeMax
            )
                                 .key,
            .mockedEntity =
                createPermissionedDomainObject(kAccount, kIndex1, kRangeMax, 0, xrpl::uint256{0}, 0)
        },
        NormalPathTestBundle{
            .testName = "CreateVaultObjectByHexString",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "vault": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256(kIndex1),
            .mockedEntity = createVault(
                kAccount,
                kAccount,
                kRangeMax,
                "XRP",
                xrpl::toBase58(xrpl::xrpAccount()),
                xrpl::uint192(0),
                0,
                xrpl::uint256{0},
                0
            )
        },
        NormalPathTestBundle{
            .testName = "CreateVaultObjectByAccount",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "vault": {{
                        "owner": "{}",
                        "seq": {}
                    }}
                }})JSON",
                kAccount,
                kRangeMax
            ),
            .expectedIndex =
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            xrpl::keylet::vault(*xrpl::parseBase58<xrpl::AccountID>(kAccount), kRangeMax).key,
            .mockedEntity = createVault(
                kAccount,
                kAccount,
                kRangeMax,
                "XRP",
                xrpl::toBase58(xrpl::xrpAccount()),
                xrpl::uint192(0),
                0,
                xrpl::uint256{0},
                0
            )
        },
        NormalPathTestBundle{
            .testName = "CreateLoanBrokerObjectByHexString",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "loan_broker": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256(kIndex1),
            .mockedEntity = createLoanBroker(
                kAccount, kAccount, kRangeMax, xrpl::uint256{kIndex1}, 1, xrpl::uint256{0}, 0
            )
        },
        NormalPathTestBundle{
            .testName = "CreateLoanBrokerObjectByOwnerAndSeq",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "loan_broker": {{
                        "owner": "{}",
                        "seq": {}
                    }}
                }})JSON",
                kAccount,
                kRangeMax
            ),
            .expectedIndex = xrpl::keylet::loanbroker(
                                 // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                 *xrpl::parseBase58<xrpl::AccountID>(kAccount),
                                 kRangeMax
            )
                                 .key,
            .mockedEntity = createLoanBroker(
                kAccount, kAccount, kRangeMax, xrpl::uint256{kIndex1}, 1, xrpl::uint256{0}, 0
            )
        },
        NormalPathTestBundle{
            .testName = "CreateLoanObjectByHexString",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "loan": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256(kIndex1),
            .mockedEntity = createLoan(
                kAccount, xrpl::uint256{kIndex1}, 1, 1000, 86400, 100, xrpl::uint256{0}, 0
            )
        },
        NormalPathTestBundle{
            .testName = "CreateLoanObjectByLoanBrokerIdAndSeq",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "loan": {{
                        "loan_broker_id": "{}",
                        "loan_seq": 1
                    }}
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::keylet::loan(xrpl::uint256{kIndex1}, 1).key,
            .mockedEntity = createLoan(
                kAccount, xrpl::uint256{kIndex1}, 1, 1000, 86400, 100, xrpl::uint256{0}, 0
            )
        },
        NormalPathTestBundle{
            .testName = "DelegateViaStringIndex",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "delegate": "{}"
                }})JSON",
                kIndex1
            ),
            .expectedIndex = xrpl::uint256{kIndex1},
            .mockedEntity =
                createDelegateObject(kAccount, kAccount2, kIndex1, 0, xrpl::uint256{0}, 0)
        },
        NormalPathTestBundle{
            .testName = "DelegateViaObject",
            .testJson = fmt::format(
                R"JSON({{
                    "binary": true,
                    "delegate": {{
                        "account": "{}",
                        "authorize": "{}"
                    }}
                }})JSON",
                kAccount,
                kAccount2
            ),
            .expectedIndex = xrpl::keylet::delegate(
                                 getAccountIdWithString(kAccount), getAccountIdWithString(kAccount2)
            )
                                 .key,
            .mockedEntity =
                createDelegateObject(kAccount, kAccount2, kIndex1, 0, xrpl::uint256{0}, 0)
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup2,
    RPCLedgerEntryNormalPathTest,
    ValuesIn(generateTestValuesForNormalPathTest()),
    tests::util::kNameGenerator
);

// Test for normal path
// Check the index in response matches the computed index accordingly
TEST_P(RPCLedgerEntryNormalPathTest, NormalPath)
{
    auto const testBundle = GetParam();

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    EXPECT_CALL(*backend_, doFetchLedgerObject(testBundle.expectedIndex, kRangeMax, _))
        .WillRepeatedly(Return(testBundle.mockedEntity.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const& outputJson = output.result.value();
        EXPECT_EQ(outputJson.at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(outputJson.at("ledger_index").as_uint64(), kRangeMax);
        EXPECT_EQ(
            outputJson.at("node_binary").as_string(),
            xrpl::strHex(testBundle.mockedEntity.getSerializer().peekData())
        );
        EXPECT_EQ(
            xrpl::uint256(boost::json::value_to<std::string>(outputJson.at("index")).data()),
            testBundle.expectedIndex
        );
    });
}

// this testcase will test the deserialization of ledger entry
TEST_F(RPCLedgerEntryTest, BinaryFalse)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount": "100",
            "Balance": "200",
            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Flags": 0,
            "LedgerEntryType": "PayChannel",
            "OwnerNode": "0",
            "PreviousTxnID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq": 400,
            "PublicKey": "020000000000000000000000000000000000000000000000000000000000000000",
            "SettleDelay": 300,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
        }
    })JSON";

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 200, 300, kIndex1, 400);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "payment_channel": "{}"
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

TEST_F(RPCLedgerEntryTest, Vault_BinaryFalse)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    boost::json::object const entry;

    auto const vault = createVault(
        kAccount,
        kAccount,
        kRangeMax,
        "XRP",
        xrpl::toBase58(xrpl::xrpAccount()),
        xrpl::uint192(0),
        0,
        xrpl::uint256{1},
        0
    );

    auto const vaultKey =
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        xrpl::keylet::vault(*xrpl::parseBase58<xrpl::AccountID>(kAccount), kRangeMax).key;

    xrpl::STLedgerEntry const sle{
        xrpl::SerialIter{
            vault.getSerializer().peekData().data(), vault.getSerializer().peekData().size()
        },
        vaultKey
    };

    EXPECT_CALL(*backend_, doFetchLedgerObject(vaultKey, testing::_, testing::_))
        .WillOnce(Return(vault.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "binary": false,
                    "vault": {{
                        "owner": "{}",
                        "seq": {}
                    }}
                }})JSON",
                kAccount,
                kRangeMax
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ(output.result->at("node").at("Owner").as_string(), kAccount);
        EXPECT_EQ(output.result->at("node").at("Sequence").as_int64(), kRangeMax);
    });
}

TEST_F(RPCLedgerEntryTest, LoanBroker_BinaryFalse)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    boost::json::object const entry;

    auto const loanBroker = createLoanBroker(
        kAccount, kAccount, kRangeMax, xrpl::uint256{kIndex1}, 1, xrpl::uint256{1}, 0
    );

    auto const loanBrokerKey = xrpl::keylet::loanbroker(
                                   // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                   *xrpl::parseBase58<xrpl::AccountID>(kAccount),
                                   kRangeMax
    )
                                   .key;

    xrpl::STLedgerEntry const sle{
        xrpl::SerialIter{
            loanBroker.getSerializer().peekData().data(),
            loanBroker.getSerializer().peekData().size()
        },
        loanBrokerKey
    };

    EXPECT_CALL(*backend_, doFetchLedgerObject(loanBrokerKey, testing::_, testing::_))
        .WillOnce(Return(loanBroker.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "binary": false,
                    "loan_broker": {{
                        "owner": "{}",
                        "seq": {}
                    }}
                }})JSON",
                kAccount,
                kRangeMax
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ(output.result->at("node").at("Owner").as_string(), kAccount);
        EXPECT_EQ(output.result->at("node").at("Sequence").as_int64(), kRangeMax);
        EXPECT_EQ(output.result->at("node").at("LoanSequence").as_int64(), 1);
    });
}

TEST_F(RPCLedgerEntryTest, Loan_BinaryFalse)
{
    static constexpr auto kLoanSeq = 1;
    static constexpr auto kStartDate = 1000;
    static constexpr auto kPaymentInterval = 86400;
    static constexpr auto kInterestRate = 100;

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    boost::json::object const entry;

    auto const loan = createLoan(
        kAccount,
        xrpl::uint256{kIndex1},
        kLoanSeq,
        kStartDate,
        kPaymentInterval,
        kInterestRate,
        xrpl::uint256{1},
        0
    );

    EXPECT_CALL(*backend_, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillOnce(Return(loan.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "binary": false,
                    "loan": {{
                        "loan_broker_id": "{}",
                        "loan_seq": {}
                    }}
                }})JSON",
                kIndex1,
                kLoanSeq
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ(output.result->at("node").at("Borrower").as_string(), kAccount);
        EXPECT_EQ(output.result->at("node").at("LoanSequence").as_int64(), kLoanSeq);
        EXPECT_EQ(output.result->at("node").at("StartDate").as_int64(), kStartDate);
        EXPECT_EQ(output.result->at("node").at("PaymentInterval").as_int64(), kPaymentInterval);
    });
}

TEST_F(RPCLedgerEntryTest, UnexpectedLedgerType)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 200, 300, kIndex1, 400);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "check": "{}"
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "unexpectedLedgerType");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "check": "{}",
                    "ledger_index": {}
                }})JSON",
                kIndex1,
                kRangeMax
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "check": "{}",
                    "ledger_index": "{}"
                }})JSON",
                kIndex1,
                kRangeMax
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "check": "{}",
                    "ledger_hash": "{}"
                }})JSON",
                kIndex1,
                kLedgerHash
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, InvalidEntryTypeVersion2)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({})JSON");
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "No ledger_entry params provided.");
    });
}

TEST_F(RPCLedgerEntryTest, InvalidEntryTypeVersion1)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(R"JSON({})JSON");
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "unknownOption");
        EXPECT_EQ(err.at("error_message").as_string(), "Unknown option.");
    });
}

TEST(RPCLedgerEntrySpecTest, DeprecatedFields)
{
    boost::json::value const json{{"ledger", 2}};
    auto const spec = LedgerEntryHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(
        warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated)
    );
    EXPECT_NE(
        warning.at("message").as_string().find("Field 'ledger' is deprecated."), std::string::npos
    ) << warning;
}

// Same as BinaryFalse with include_deleted set to true
// Expected Result: same as BinaryFalse
TEST_F(RPCLedgerEntryTest, BinaryFalseIncludeDeleted)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount": "100",
            "Balance": "200",
            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Flags": 0,
            "LedgerEntryType": "PayChannel",
            "OwnerNode": "0",
            "PreviousTxnID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq": 400,
            "PublicKey": "020000000000000000000000000000000000000000000000000000000000000000",
            "SettleDelay": 300,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
        }
    })JSON";

    // return valid ledgerinfo
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 200, 300, kIndex1, 400);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

// Test for object is deleted in the latest sequence
// Expected Result: return the latest object that is not deleted
TEST_F(RPCLedgerEntryTest, LedgerEntryDeleted)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "deleted_ledger_index": 30,
        "node": {
            "Amount": "123",
            "Flags": 0,
            "LedgerEntryType": "NFTokenOffer",
            "NFTokenID": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
            "NFTokenOfferNode": "0",
            "Owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
            }
        })JSON";
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));
    // return valid ledger entry which can be deserialized
    auto const offer = createNftBuyOffer(kNftId, kAccount);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObjectSeq(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(uint32_t{kRangeMax}));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax - 1, _))
        .WillOnce(Return(offer.getSerializer().peekData()));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

// Test for object not exist in database
// Expected Result: return entryNotFound error
TEST_F(RPCLedgerEntryTest, LedgerEntryNotExist)
{
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObjectSeq(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(uint32_t{kRangeMax}));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax - 1, _))
        .WillOnce(Return(std::optional<Blob>{}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        auto const myerr = err.at("error").as_string();
        EXPECT_EQ(myerr, "entryNotFound");
    });
}

// Same as BinaryFalse with include_deleted set to false
// Expected Result: same as BinaryFalse
TEST_F(RPCLedgerEntryTest, BinaryFalseIncludeDeleteFalse)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount": "100",
            "Balance": "200",
            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Flags": 0,
            "LedgerEntryType": "PayChannel",
            "OwnerNode": "0",
            "PreviousTxnID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq": 400,
            "PublicKey": "020000000000000000000000000000000000000000000000000000000000000000",
            "SettleDelay": 300,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
        }
    })JSON";

    // return valid ledgerinfo
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry =
        createPaymentChannelLedgerObject(kAccount, kAccount2, 100, 200, 300, kIndex1, 400);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "payment_channel": "{}",
                    "include_deleted": false
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

// Test when an object is updated and include_deleted is set to true
// Expected Result: return the latest object that is not deleted (latest object in this test)
TEST_F(RPCLedgerEntryTest, ObjectUpdateIncludeDelete)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node": {
            "Balance": {
                "currency": "USD",
                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value": "10"
            },
            "Flags": 0,
            "HighLimit": {
                "currency": "USD",
                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value": "200"
            },
            "LedgerEntryType": "RippleState",
            "LowLimit": {
                "currency": "USD",
                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "value": "100"
            },
            "PreviousTxnID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq": 123,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
            }
        })JSON";

    // return valid ledgerinfo
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const line1 = createRippleStateLedgerObject(
        "USD", kAccount2, 10, kAccount, 100, kAccount2, 200, kTxnId, 123
    );
    auto const line2 = createRippleStateLedgerObject(
        "USD", kAccount, 10, kAccount2, 100, kAccount, 200, kTxnId, 123
    );
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillRepeatedly(Return(line1.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax - 1, _))
        .WillRepeatedly(Return(line2.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

// Test when an object is deleted several sequence ago and include_deleted is set to true
// Expected Result: return the latest object that is not deleted
TEST_F(RPCLedgerEntryTest, ObjectDeletedPreviously)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "deleted_ledger_index": 26,
        "node": {
            "Amount": "123",
            "Flags": 0,
            "LedgerEntryType": "NFTokenOffer",
            "NFTokenID": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
            "NFTokenOfferNode": "0",
            "Owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "index": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
            }
        })JSON";
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));
    // return valid ledger entry which can be deserialized
    auto const offer = createNftBuyOffer(kNftId, kAccount);
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObjectSeq(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(uint32_t{kRangeMax - 4}));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax - 5, _))
        .WillOnce(Return(offer.getSerializer().peekData()));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}

// Test for object seq not exist in database
// Expected Result: return entryNotFound error
TEST_F(RPCLedgerEntryTest, ObjectSeqNotExist)
{
    auto const ledgerinfo = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillRepeatedly(Return(ledgerinfo));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObjectSeq(xrpl::uint256{kIndex1}, kRangeMax, _))
        .WillOnce(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "index": "{}",
                    "include_deleted": true
                }})JSON",
                kIndex1
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        auto const myerr = err.at("error").as_string();
        EXPECT_EQ(myerr, "entryNotFound");
    });
}

// this testcase will test the if response includes synthetic mpt_issuance_id
TEST_F(RPCLedgerEntryTest, SyntheticMPTIssuanceID)
{
    static constexpr auto kOut = R"JSON({
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "index": "FD7E7EFAE2A20E75850D0E0590B205E2F74DC472281768CD6E03988069816336",
        "node": {
            "Flags": 0,
            "Issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "LedgerEntryType": "MPTokenIssuance",
            "MPTokenMetadata": "6D65746164617461",
            "OutstandingAmount": "0",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "Sequence": 2,
            "index": "FD7E7EFAE2A20E75850D0E0590B205E2F74DC472281768CD6E03988069816336",
            "mpt_issuance_id": "000000024B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
        }
    })JSON";

    auto const mptId = xrpl::makeMptID(2, getAccountIdWithString(kAccount));

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = createMptIssuanceObject(kAccount, 2, "metadata");
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::mptIssuance(mptId).key, kRangeMax, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "mpt_issuance": "{}"
                }})JSON",
                xrpl::to_string(mptId)
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(kOut));
    });
}
