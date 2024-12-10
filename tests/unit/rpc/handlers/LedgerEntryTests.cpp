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
#include <fmt/core.h>
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
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto INDEX1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto ACCOUNT3 = "rhzcyub9SbyZ4YF1JYskN5rLrTDUuLZG6D";
constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto TOKENID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";
constexpr static auto NFTID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr static auto TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto CREDENTIALTYPE = "4B5943";

struct RPCLedgerEntryTest : HandlerBaseTest {
    RPCLedgerEntryTest()
    {
        backend->setRange(RANGEMIN, RANGEMAX);
    }
};

struct ParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerEntryParameterTest : public RPCLedgerEntryTest, public WithParamInterface<ParamTestCaseBundle> {};

// TODO: because we extract the error generation from the handler to framework
// the error messages need one round fine tuning
static auto
generateTestValuesForParametersTest()
{
    return std::vector<ParamTestCaseBundle>{
        ParamTestCaseBundle{
            .testName = "InvalidBinaryType",
            .testJson = R"({
                "index":
                "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
                "binary": "invalid"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAccountRootFormat",
            .testJson = R"({
                "account_root": "invalid"
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDidFormat",
            .testJson = R"({
                "did": "invalid"
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAccountRootNotString",
            .testJson = R"({
                "account_root": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "account_rootNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidLedgerIndex",
            .testJson = R"({
                "ledger_index": "wrong"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },

        ParamTestCaseBundle{
            .testName = "UnknownOption",
            .testJson = R"({})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthType",
            .testJson = R"({
                "deposit_preauth": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthString",
            .testJson = R"({
                "deposit_preauth": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthEmtpyJson",
            .testJson = R"({
                "deposit_preauth": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonWrongAccount",
            .testJson = R"({
                "deposit_preauth": {
                    "owner": "invalid",
                    "authorized": "invalid"
                }
            })",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonOwnerNotString",
            .testJson = R"({
                "deposit_preauth": {
                    "owner": 123,
                    "authorized": 123
                }
            })",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizedNotString",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": 123
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "authorizedNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizeCredentialsNotArray",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": "asdf"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "authorized_credentials not array"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDepositPreauthJsonAuthorizeCredentialsMalformedString",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": ["C2F2A19C8D0D893D18F18FDCFE13A3ECB41767E48422DF07F2455CDA08FDF09B"]
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "authorized_credentials elements in array are not objects."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthBothAuthAndAuthCredentialsDoesNotExists",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Must have one of authorized or authorized_credentials."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthBothAuthAndAuthCredentialsExists",
            .testJson = fmt::format(
                R"({{
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
                }})",
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT3,
                CREDENTIALTYPE
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Must have one of authorized or authorized_credentials."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthEmptyAuthorizeCredentials",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        ]
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Requires at least one element in authorized_credentials array."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsMissingCredentialType",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                            {{
                                "issuer": "{}"
                            }}
                        ]
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Field 'CredentialType' is required but missing."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsMissingIssuer",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "credential_type": "{}"
                        }}
                        ]
                    }}
                }})",
                ACCOUNT,
                CREDENTIALTYPE
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "Field 'Issuer' is required but missing."
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsIncorrectIssuerType",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": 123,
                            "credential_type": "{}"
                        }}
                        ]
                    }}
                }})",
                ACCOUNT,
                CREDENTIALTYPE
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "issuer NotString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsIncorrectCredentialType",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": 432
                        }}
                        ]
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type NotString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsCredentialTypeNotHex",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": "hello world"
                        }}
                        ]
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type NotHexString"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthAuthorizeCredentialsCredentialTypeEmpty",
            .testJson = fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized_credentials": [
                        {{
                            "issuer": "{}",
                            "credential_type": ""
                        }}
                        ]
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "credential_type is empty"
        },

        ParamTestCaseBundle{
            .testName = "DepositPreauthDuplicateAuthorizeCredentials",
            .testJson = fmt::format(
                R"({{
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
                }})",
                ACCOUNT,
                ACCOUNT2,
                CREDENTIALTYPE,
                ACCOUNT2,
                CREDENTIALTYPE
            ),
            .expectedError = "malformedAuthorizedCredentials",
            .expectedErrorMessage = "duplicates in credentials."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketType",
            .testJson = R"({
                "ticket": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketIndex",
            .testJson = R"({
                "ticket": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketEmptyJson",
            .testJson = R"({
                "ticket": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonAccountNotString",
            .testJson = R"({
                "ticket": {
                    "account": 123,
                    "ticket_seq": 123
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonAccountInvalid",
            .testJson = R"({
                "ticket": {
                    "account": "123",
                    "ticket_seq": 123
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidTicketJsonSeqNotInt",
            .testJson = fmt::format(
                R"({{
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferType",
            .testJson = R"({
                "offer": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferIndex",
            .testJson = R"({
                "offer": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferEmptyJson",
            .testJson = R"({
                "offer": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonAccountNotString",
            .testJson = R"({
                "ticket": {
                    "account": 123,
                    "seq": 123
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonAccountInvalid",
            .testJson = R"({
                "ticket": {
                    "account": "123",
                    "seq": 123
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidOfferJsonSeqNotInt",
            .testJson = fmt::format(
                R"({{
                    "offer": {{
                        "account": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowType",
            .testJson = R"({
                "escrow": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowIndex",
            .testJson = R"({
                "escrow": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowEmptyJson",
            .testJson = R"({
                "escrow": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonAccountNotString",
            .testJson = R"({
                "escrow": {
                    "owner": 123,
                    "seq": 123
                }
            })",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonAccountInvalid",
            .testJson = R"({
                "escrow": {
                    "owner": "123",
                    "seq": 123
                }
            })",
            .expectedError = "malformedOwner",
            .expectedErrorMessage = "Malformed owner."
        },

        ParamTestCaseBundle{
            .testName = "InvalidEscrowJsonSeqNotInt",
            .testJson = fmt::format(
                R"({{
                    "escrow": {{
                        "owner": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateType",
            .testJson = R"({
                "ripple_state": "123"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateMissField",
            .testJson = R"({
                "ripple_state": {
                    "currency": "USD"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateEmtpyJson",
            .testJson = R"({
                "ripple_state": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateOneAccount",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}"]
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateSameAccounts",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongAccountsNotString",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}",123],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "malformedAccounts"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongAccountsFormat",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","123"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "malformedAddresses"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongCurrency",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "XXXX"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedCurrency",
            .expectedErrorMessage = "malformedCurrency"
        },

        ParamTestCaseBundle{
            .testName = "InvalidRippleStateWrongCurrencyNotString",
            .testJson = fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": 123
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "currencyNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryType",
            .testJson = R"({
                "directory": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryIndex",
            .testJson = R"({
                "directory": "123"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryEmtpyJson",
            .testJson = R"({
                "directory": {}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "missingOwnerOrDirRoot"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongOwnerNotString",
            .testJson = R"({
                "directory": {
                    "owner": 123
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ownerNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongOwnerFormat",
            .testJson = R"({
                "directory": {
                    "owner": "123"
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongDirFormat",
            .testJson = R"({
                "directory": {
                    "dir_root": "123"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "dir_rootMalformed"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryWrongDirNotString",
            .testJson = R"({
                "directory": {
                    "dir_root": 123
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "dir_rootNotString"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryDirOwnerConflict",
            .testJson = fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "owner": "{}"
                    }}
                }})",
                INDEX1,
                ACCOUNT
            ),
            .expectedError = "invalidParams",
            .expectedErrorMessage = "mayNotSpecifyBothDirRootAndOwner"
        },

        ParamTestCaseBundle{
            .testName = "InvalidDirectoryDirSubIndexNotInt",
            .testJson = fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": "not int"
                    }}
                }})",
                INDEX1
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "InvalidAMMStringIndex",
            .testJson = R"({
                "amm": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMJson",
            .testJson = R"({
                "amm": {}
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "NonObjectAMMJsonAsset",
            .testJson = R"({
                "amm": {
                    "asset": 123,
                    "asset2": 123
                }
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMAssetJson",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset":{{}},
                        "asset2":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "EmptyAMMAsset2Json",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":{{}},
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAsset2Json",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetJson",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAssetNotJson",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset": "invalid",
                        "asset2":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAsset2NotJson",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2": "invalid",
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "WrongAMMAssetCurrency",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency":"XRP"
                        }},
                        "asset":
                        {{
                            "currency" : "USD2",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "WrongAMMAssetIssuer",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency":"XRP"
                        }},
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "aa{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetIssuerForNonXRP",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency":"JPY"
                        }},
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "AMMAssetHasIssuerForXRP",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency":"XRP",
                            "issuer":"{}"
                        }},
                        "asset":
                        {{
                            "currency" : "USD",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },

        ParamTestCaseBundle{
            .testName = "MissingAMMAssetCurrency",
            .testJson = fmt::format(
                R"({{
                    "amm": 
                    {{
                        "asset2":
                        {{
                            "currency":"XRP"
                        }},
                        "asset":
                        {{
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingBridgeAccount",
            .testJson = fmt::format(
                R"({{
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeCurrencyIsNumber",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": {},
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT,
                1,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuerIsNumber",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": {}
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT,
                "JPY",
                2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuingChainIssueIsNotObject",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue": 1
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeWithInvalidBridgeAccount",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "abcd",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeDoorInvalid",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "abcd",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssuerInvalid",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "invalid"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueCurrencyInvalid",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "JPJPJP",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueXRPCurrencyInvalid",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP",
                            "issuer": "{}"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT2,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeIssueJPYCurrencyInvalid",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "JPY"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingLockingChainDoor",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP",
                            "issuer": "{}"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT2,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingIssuingChainDoor",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "LockingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingLockingChainIssue",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "IssuingChainDoor": "{}",
                        "LockingChainDoor": "{}",
                        "IssuingChainIssue":
                        {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeMissingIssuingChainIssue",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": 
                    {{
                        "IssuingChainDoor": "{}",
                        "LockingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "JPY",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "BridgeInvalidType",
            .testJson = fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": "invalid"
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdInvalidType",
            .testJson = R"({
                "xchain_owned_claim_id": 123
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingClaimId",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_claim_id": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingDoor",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_claim_id": 
                    {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonMissingIssue",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_claim_id": 
                    {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT
            ),

            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonInvalidDoor",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_claim_id": 
                    {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "abcd",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedClaimIdJsonInvalidIssue",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_claim_id": 
                    {{
                        "xchain_owned_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdInvalidType",
            .testJson = R"({
                    "xchain_owned_create_account_claim_id": 123
                    })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingClaimId",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_create_account_claim_id": 
                    {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingDoor",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_create_account_claim_id": 
                    {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonMissingIssue",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_create_account_claim_id": 
                    {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT
            ),

            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonInvalidDoor",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_create_account_claim_id": 
                    {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "abcd",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}",
                            "issuer": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                "JPY",
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OwnedCreateAccountClaimIdJsonInvalidIssue",
            .testJson = fmt::format(
                R"({{
                    "xchain_owned_create_account_claim_id": 
                    {{
                        "xchain_owned_create_account_claim_id": 10,
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue":
                        {{
                            "currency": "XRP"
                        }},
                        "IssuingChainIssue":
                        {{
                            "currency": "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                "JPY"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdMissing",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidNegative",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": -1
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeString",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": "invalid"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeDouble",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 3.21
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeObject",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": {{}}
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeArray",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": []
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectDocumentIdInvalidTypeNull",
            .testJson = fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": null
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedDocumentID",
            .expectedErrorMessage = "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountMissing",
            .testJson = R"({
                "oracle": {
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeInteger",
            .testJson = R"({
                "oracle": {
                    "account": 123,
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeDouble",
            .testJson = R"({
                "oracle": {
                    "account": 123.45,
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeNull",
            .testJson = R"({
                "oracle": {
                    "account": null,
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeObject",
            .testJson = R"({
                "oracle": {
                    "account": {"test": "test"},
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidTypeArray",
            .testJson = R"({
                "oracle": {
                    "account": [{"test": "test"}],
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleObjectAccountInvalidFormat",
            .testJson = R"({
                "oracle": {
                    "account": "NotHex",
                    "oracle_document_id": 1
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidFormat",
            .testJson = R"({
                "oracle": "NotHex"
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeInteger",
            .testJson = R"({
                "oracle": 123
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeDouble",
            .testJson = R"({
                "oracle": 123.45
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeArray",
            .testJson = R"({
                "oracle": [{"test": "test"}]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "OracleStringInvalidTypeNull",
            .testJson = R"({
                "oracle": null
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidSubjectType",
            .testJson = R"({
                "credential": {
                    "subject": 123
                }
            })",
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidIssuerType",
            .testJson = fmt::format(
                R"({{
                "credential": {{
                    "issuer": ["{}"]
                }}
            }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTIssuanceStringIndex",
            .testJson = R"({
                "mpt_issuance": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTIssuanceType",
            .testJson = R"({
                "mpt_issuance": 0
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenStringIndex",
            .testJson = R"({
                "mptoken": "invalid"
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenObject",
            .testJson = fmt::format(
                R"({{
                    "mptoken": {{}}
                }})"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "MissingMPTokenID",
            .testJson = fmt::format(
                R"({{
                    "mptoken": {{
                        "account": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialInvalidCredentialType",
            .testJson = fmt::format(
                R"({{
                "credential": {{
                    "subject": "{}",
                    "issuer": "{}",
                    "credential_type": 1234
                }}
            }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "CredentialMissingIssuerField",
            .testJson = fmt::format(
                R"({{
                "credential": {{
                    "subject": "{}",
                    "credential_type": "1234"
                }}
            }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenAccount",
            .testJson = fmt::format(
                R"({{
                    "mptoken": {{
                        "mpt_issuance_id": "0000019315EABA24E6135A4B5CE2899E0DA791206413B33D",
                        "account": 1
                    }}
                }})"
            ),
            .expectedError = "malformedAddress",
            .expectedErrorMessage = "Malformed address."
        },
        ParamTestCaseBundle{
            .testName = "InvalidMPTokenType",
            .testJson = fmt::format(
                R"({{
                    "mptoken": 0
                }})"
            ),
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup1,
    LedgerEntryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(LedgerEntryParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
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

// content of index, payment_channel, nft_page and check fields is ledger index.
INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup3,
    IndexTest,
    Values("index", "nft_page", "payment_channel", "check"),
    IndexTest::NameGenerator{}
);

TEST_P(IndexTest, InvalidIndexUint256)
{
    auto const index = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "{}": "invalid"
            }})",
            index
        ));
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
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "{}": 123
            }})",
            index
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "malformedRequest");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed request.");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerEntryNotFound)
{
    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerHeader));

    // return null for ledger entry
    auto const key = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(key, RANGEMAX, _)).WillRepeatedly(Return(std::optional<Blob>{}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "account_root": "{}"
            }})",
            ACCOUNT
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
    });
}

struct NormalPathTestBundle {
    std::string testName;
    std::string testJson;
    ripple::uint256 expectedIndex;
    ripple::STObject mockedEntity;
};

struct RPCLedgerEntryNormalPathTest : public RPCLedgerEntryTest, public WithParamInterface<NormalPathTestBundle> {};

static auto
generateTestValuesForNormalPathTest()
{
    auto account1 = GetAccountIDWithString(ACCOUNT);
    auto account2 = GetAccountIDWithString(ACCOUNT2);
    ripple::Currency currency;
    ripple::to_currency(currency, "USD");

    return std::vector<NormalPathTestBundle>{
        NormalPathTestBundle{
            .testName = "Index",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "index": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateAccountRootObject(ACCOUNT2, ripple::lsfGlobalFreeze, 1, 10, 2, INDEX1, 3)
        },
        NormalPathTestBundle{
            .testName = "Payment_channel",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "payment_channel": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400)
        },
        NormalPathTestBundle{
            .testName = "Nft_page",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "nft_page": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateNFTTokenPage(
                std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt
            )
        },
        NormalPathTestBundle{
            .testName = "Check",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "check": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateCheckLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "DirectoryIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "directory": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            .testName = "OfferIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "offer": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1
            )
        },
        NormalPathTestBundle{
            .testName = "EscrowIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "escrow": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "TicketIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "ticket": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateTicketLedgerObject(ACCOUNT, 0)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "deposit_preauth": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateDepositPreauthLedgerObjectByAuth(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "AccountRoot",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "account_root": "{}"
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key,
            .mockedEntity = CreateAccountRootObject(ACCOUNT, 0, 1, 1, 1, INDEX1, 1)
        },
        NormalPathTestBundle{
            .testName = "DID",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "did": "{}"
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::did(GetAccountIDWithString(ACCOUNT)).key,
            .mockedEntity = CreateDidObject(ACCOUNT, "mydocument", "myURI", "mydata")
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaDirRoot",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": 2
                    }}
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::keylet::page(ripple::uint256{INDEX1}, 2).key,
            .mockedEntity = CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaOwner",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}",
                        "sub_index": 2
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::page(ripple::keylet::ownerDir(account1), 2).key,
            .mockedEntity = CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            .testName = "DirectoryViaDefaultSubIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            // default sub_index is 0
            .expectedIndex = ripple::keylet::page(ripple::keylet::ownerDir(account1), 0).key,
            .mockedEntity = CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            .testName = "Escrow",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "escrow": {{
                        "owner": "{}",
                        "seq": 1
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::escrow(account1, 1).key,
            .mockedEntity = CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthByAuth",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": "{}"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedIndex = ripple::keylet::depositPreauth(account1, account2).key,
            .mockedEntity = CreateDepositPreauthLedgerObjectByAuth(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "DepositPreauthByAuthCredentials",
            .testJson = fmt::format(
                R"({{
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
                   }})",
                ACCOUNT,
                ACCOUNT2,
                CREDENTIALTYPE
            ),
            .expectedIndex =
                ripple::keylet::depositPreauth(
                    account1,
                    credentials::createAuthCredentials(CreateAuthCredentialArray(
                        std::vector<std::string_view>{ACCOUNT2}, std::vector<std::string_view>{CREDENTIALTYPE}
                    ))
                )
                    .key,
            .mockedEntity = CreateDepositPreauthLedgerObjectByAuthCredentials(ACCOUNT, ACCOUNT2, CREDENTIALTYPE)
        },
        NormalPathTestBundle{
            .testName = "Credentials",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "credential": {{
                        "subject": "{}",
                        "issuer": "{}",
                        "credential_type": "{}"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2,
                CREDENTIALTYPE
            ),
            .expectedIndex =
                ripple::keylet::credential(
                    account1,
                    account2,
                    ripple::Slice(ripple::strUnHex(CREDENTIALTYPE)->data(), ripple::strUnHex(CREDENTIALTYPE)->size())
                )
                    .key,
            .mockedEntity = CreateCredentialObject(ACCOUNT, ACCOUNT2, CREDENTIALTYPE)
        },
        NormalPathTestBundle{
            .testName = "RippleState",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "ripple_state": {{
                        "accounts": ["{}","{}"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            .expectedIndex = ripple::keylet::line(account1, account2, currency).key,
            .mockedEntity =
                CreateRippleStateLedgerObject("USD", ACCOUNT2, 100, ACCOUNT, 10, ACCOUNT2, 20, INDEX1, 123, 0)
        },
        NormalPathTestBundle{
            .testName = "Ticket",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": 2
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::getTicketIndex(account1, 2),
            .mockedEntity = CreateTicketLedgerObject(ACCOUNT, 0)
        },
        NormalPathTestBundle{
            .testName = "Offer",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "offer": {{
                        "account": "{}",
                        "seq": 2
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::offer(account1, 2).key,
            .mockedEntity = CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1
            )
        },
        NormalPathTestBundle{
            .testName = "AMMViaIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "amm": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateAMMObject(ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "AMMViaJson",
            .testJson = fmt::format(
                R"({{
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
                }})",
                "JPY",
                ACCOUNT2
            ),
            .expectedIndex =
                ripple::keylet::amm(GetIssue("XRP", ripple::toBase58(ripple::xrpAccount())), GetIssue("JPY", ACCOUNT2))
                    .key,
            .mockedEntity = CreateAMMObject(ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", ACCOUNT2)
        },
        NormalPathTestBundle{
            .testName = "BridgeLocking",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency" : "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency" : "JPY",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT,
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT3
            ),
            .expectedIndex = ripple::keylet::bridge(
                                 ripple::STXChainBridge(
                                     GetAccountIDWithString(ACCOUNT),
                                     ripple::xrpIssue(),
                                     GetAccountIDWithString(ACCOUNT2),
                                     GetIssue("JPY", ACCOUNT3)
                                 ),
                                 ripple::STXChainBridge::ChainType::locking
            )
                                 .key,
            .mockedEntity = CreateBridgeObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3)
        },
        NormalPathTestBundle{
            .testName = "BridgeIssuing",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "bridge_account": "{}",
                    "bridge": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency" : "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency" : "JPY",
                            "issuer" : "{}"
                        }}
                    }}
                }})",
                ACCOUNT2,
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT3
            ),
            .expectedIndex = ripple::keylet::bridge(
                                 ripple::STXChainBridge(
                                     GetAccountIDWithString(ACCOUNT),
                                     ripple::xrpIssue(),
                                     GetAccountIDWithString(ACCOUNT2),
                                     GetIssue("JPY", ACCOUNT3)
                                 ),
                                 ripple::STXChainBridge::ChainType::issuing
            )
                                 .key,
            .mockedEntity = CreateBridgeObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3)
        },
        NormalPathTestBundle{
            .testName = "XChainOwnedClaimId",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "xchain_owned_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency" : "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency" : "JPY",
                            "issuer" : "{}"
                        }},
                        "xchain_owned_claim_id": 10
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT3
            ),
            .expectedIndex = ripple::keylet::xChainClaimID(
                                 ripple::STXChainBridge(
                                     GetAccountIDWithString(ACCOUNT),
                                     ripple::xrpIssue(),
                                     GetAccountIDWithString(ACCOUNT2),
                                     GetIssue("JPY", ACCOUNT3)
                                 ),
                                 10
            )
                                 .key,
            .mockedEntity = CreateChainOwnedClaimIDObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3, ACCOUNT)
        },
        NormalPathTestBundle{
            .testName = "XChainOwnedCreateAccountClaimId",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "xchain_owned_create_account_claim_id": {{
                        "LockingChainDoor": "{}",
                        "IssuingChainDoor": "{}",
                        "LockingChainIssue": {{
                            "currency" : "XRP"
                        }},
                        "IssuingChainIssue": {{
                            "currency" : "JPY",
                            "issuer" : "{}"
                        }},
                        "xchain_owned_create_account_claim_id": 10
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2,
                ACCOUNT3
            ),
            .expectedIndex = ripple::keylet::xChainCreateAccountClaimID(
                                 ripple::STXChainBridge(
                                     GetAccountIDWithString(ACCOUNT),
                                     ripple::xrpIssue(),
                                     GetAccountIDWithString(ACCOUNT2),
                                     GetIssue("JPY", ACCOUNT3)
                                 ),
                                 10
            )
                                 .key,
            .mockedEntity = CreateChainOwnedClaimIDObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3, ACCOUNT)
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaIntOracleDocumentId",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 1
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key,
            .mockedEntity = CreateOracleObject(
                ACCOUNT,
                "70726F7669646572",
                32u,
                1234u,
                ripple::Blob(8, 's'),
                ripple::Blob(8, 's'),
                RANGEMAX - 2,
                ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                CreatePriceDataSeries(
                    {CreateOraclePriceData(2e4, ripple::to_currency("XRP"), ripple::to_currency("USD"), 3)}
                )
            )
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaStrOracleDocumentId",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": "1"
                    }}
                }})",
                ACCOUNT
            ),
            .expectedIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key,
            .mockedEntity = CreateOracleObject(
                ACCOUNT,
                "70726F7669646572",
                32u,
                1234u,
                ripple::Blob(8, 's'),
                ripple::Blob(8, 's'),
                RANGEMAX - 2,
                ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                CreatePriceDataSeries(
                    {CreateOraclePriceData(2e4, ripple::to_currency("XRP"), ripple::to_currency("USD"), 3)}
                )
            )
        },
        NormalPathTestBundle{
            .testName = "OracleEntryFoundViaString",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "oracle": "{}"
                }})",
                ripple::to_string(ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key)
            ),
            .expectedIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key,
            .mockedEntity = CreateOracleObject(
                ACCOUNT,
                "70726F7669646572",
                64u,
                4321u,
                ripple::Blob(8, 'a'),
                ripple::Blob(8, 'a'),
                RANGEMAX - 4,
                ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321"},
                CreatePriceDataSeries(
                    {CreateOraclePriceData(1e3, ripple::to_currency("USD"), ripple::to_currency("XRP"), 2)}
                )
            )
        },
        NormalPathTestBundle{
            .testName = "MPTIssuance",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "mpt_issuance": "{}"
                }})",
                ripple::to_string(ripple::makeMptID(2, account1))
            ),
            .expectedIndex = ripple::keylet::mptIssuance(ripple::makeMptID(2, account1)).key,
            .mockedEntity = CreateMPTIssuanceObject(ACCOUNT, 2, "metadata")
        },
        NormalPathTestBundle{
            .testName = "MPTokenViaIndex",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "mptoken": "{}"
                }})",
                INDEX1
            ),
            .expectedIndex = ripple::uint256{INDEX1},
            .mockedEntity = CreateMPTokenObject(ACCOUNT, ripple::makeMptID(2, account1))
        },
        NormalPathTestBundle{
            .testName = "MPTokenViaObject",
            .testJson = fmt::format(
                R"({{
                    "binary": true,
                    "mptoken": {{
                        "account": "{}",
                        "mpt_issuance_id": "{}"
                    }}
                }})",
                ACCOUNT,
                ripple::to_string(ripple::makeMptID(2, account1))
            ),
            .expectedIndex = ripple::keylet::mptoken(ripple::makeMptID(2, account1), account1).key,
            .mockedEntity = CreateMPTokenObject(ACCOUNT, ripple::makeMptID(2, account1))
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup2,
    RPCLedgerEntryNormalPathTest,
    ValuesIn(generateTestValuesForNormalPathTest()),
    tests::util::NameGenerator
);

// Test for normal path
// Check the index in response matches the computed index accordingly
TEST_P(RPCLedgerEntryNormalPathTest, NormalPath)
{
    auto const testBundle = GetParam();

    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerHeader));

    EXPECT_CALL(*backend, doFetchLedgerObject(testBundle.expectedIndex, RANGEMAX, _))
        .WillRepeatedly(Return(testBundle.mockedEntity.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output.result.value().at("ledger_index").as_uint64(), RANGEMAX);
        EXPECT_EQ(
            output.result.value().at("node_binary").as_string(),
            ripple::strHex(testBundle.mockedEntity.getSerializer().peekData())
        );
        EXPECT_EQ(
            ripple::uint256(boost::json::value_to<std::string>(output.result.value().at("index")).data()),
            testBundle.expectedIndex
        );
    });
}

// this testcase will test the deserialization of ledger entry
TEST_F(RPCLedgerEntryTest, BinaryFalse)
{
    static auto constexpr OUT = R"({
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "index":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"100",
            "Balance":"200",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Flags":0,
            "LedgerEntryType":"PayChannel",
            "OwnerNode":"0",
            "PreviousTxnID":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq":400,
            "PublicKey":"020000000000000000000000000000000000000000000000000000000000000000",
            "SettleDelay":300,
            "index":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
        }
    })";

    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "payment_channel": "{}"
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCLedgerEntryTest, UnexpectedLedgerType)
{
    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}"
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "unexpectedLedgerType");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_index": {}
            }})",
            INDEX1,
            RANGEMAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_index": "{}"
            }})",
            INDEX1,
            RANGEMAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillRepeatedly(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_hash": "{}"
            }})",
            INDEX1,
            LEDGERHASH
        ));
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
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(R"({})");
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCLedgerEntryTest, InvalidEntryTypeVersion1)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(R"({})");
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
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::warnRPC_DEPRECATED));
    EXPECT_NE(warning.at("message").as_string().find("Field 'ledger' is deprecated."), std::string::npos) << warning;
}

// Same as BinaryFalse with include_deleted set to true
// Expected Result: same as BinaryFalse
TEST_F(RPCLedgerEntryTest, BinaryFalseIncludeDeleted)
{
    static auto constexpr OUT = R"({
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
    })";

    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// Test for object is deleted in the latest sequence
// Expected Result: return the latest object that is not deleted
TEST_F(RPCLedgerEntryTest, LedgerEntryDeleted)
{
    static auto constexpr OUT = R"({
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
        })";
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));
    // return valid ledger entry which can be deserialized
    auto const offer = CreateNFTBuyOffer(NFTID, ACCOUNT);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObjectSeq(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(uint32_t{RANGEMAX}));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX - 1, _))
        .WillOnce(Return(offer.getSerializer().peekData()));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// Test for object not exist in database
// Expected Result: return entryNotFound error
TEST_F(RPCLedgerEntryTest, LedgerEntryNotExist)
{
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObjectSeq(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(uint32_t{RANGEMAX}));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX - 1, _))
        .WillOnce(Return(std::optional<Blob>{}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
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
    static auto constexpr OUT = R"({
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
    })";

    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "payment_channel": "{}",
                "include_deleted": false
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// Test when an object is updated and include_deleted is set to true
// Expected Result: return the latest object that is not deleted (latest object in this test)
TEST_F(RPCLedgerEntryTest, ObjectUpdateIncludeDelete)
{
    static auto constexpr OUT = R"({
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
        })";

    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const line1 = CreateRippleStateLedgerObject("USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
    auto const line2 = CreateRippleStateLedgerObject("USD", ACCOUNT, 10, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillRepeatedly(Return(line1.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX - 1, _))
        .WillRepeatedly(Return(line2.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// Test when an object is deleted several sequence ago and include_deleted is set to true
// Expected Result: return the latest object that is not deleted
TEST_F(RPCLedgerEntryTest, ObjectDeletedPreviously)
{
    static auto constexpr OUT = R"({
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
        })";
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));
    // return valid ledger entry which can be deserialized
    auto const offer = CreateNFTBuyOffer(NFTID, ACCOUNT);
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObjectSeq(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(uint32_t{RANGEMAX - 4}));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX - 5, _))
        .WillOnce(Return(offer.getSerializer().peekData()));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// Test for object seq not exist in database
// Expected Result: return entryNotFound error
TEST_F(RPCLedgerEntryTest, ObjectSeqNotExist)
{
    auto const ledgerinfo = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillOnce(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObjectSeq(ripple::uint256{INDEX1}, RANGEMAX, _)).WillOnce(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "index": "{}",
                "include_deleted": true
            }})",
            INDEX1
        ));
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
    static auto constexpr OUT = R"({
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "index":"FD7E7EFAE2A20E75850D0E0590B205E2F74DC472281768CD6E03988069816336",
        "node":{
            "Flags":0,
            "Issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "LedgerEntryType":"MPTokenIssuance",
            "MPTokenMetadata":"6D65746164617461",
            "MaximumAmount":"0",
            "OutstandingAmount":"0",
            "OwnerNode":"0",
            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq":0,
            "Sequence":2,
            "index":"FD7E7EFAE2A20E75850D0E0590B205E2F74DC472281768CD6E03988069816336",
            "mpt_issuance_id":"000000024B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
        }
    })";

    auto const mptId = ripple::makeMptID(2, GetAccountIDWithString(ACCOUNT));

    // return valid ledgerHeader
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerHeader));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreateMPTIssuanceObject(ACCOUNT, 2, "metadata");
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::mptIssuance(mptId).key, RANGEMAX, _))
        .WillRepeatedly(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "mpt_issuance": "{}"
            }})",
            ripple::to_string(mptId)
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}
