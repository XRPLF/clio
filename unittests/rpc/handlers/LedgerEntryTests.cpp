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
#include "rpc/handlers/LedgerEntry.hpp"
#include "util/Fixtures.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STXChainBridge.h>
#include <ripple/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
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

class RPCLedgerEntryTest : public HandlerBaseTest {};

struct ParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerEntryParameterTest : public RPCLedgerEntryTest, public WithParamInterface<ParamTestCaseBundle> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<ParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

// TODO: because we extract the error generation from the handler to framework
// the error messages need one round fine tuning
static auto
generateTestValuesForParametersTest()
{
    return std::vector<ParamTestCaseBundle>{
        ParamTestCaseBundle{
            "InvalidBinaryType",
            R"({
                "index":
                "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
                "binary": "invalid"
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidAccountRootFormat",
            R"({
                "account_root": "invalid"
            })",
            "malformedAddress",
            "Malformed address."
        },

        ParamTestCaseBundle{
            "InvalidDidFormat",
            R"({
                "did": "invalid"
            })",
            "malformedAddress",
            "Malformed address."
        },

        ParamTestCaseBundle{
            "InvalidAccountRootNotString",
            R"({
                "account_root": 123
            })",
            "invalidParams",
            "account_rootNotString"
        },

        ParamTestCaseBundle{
            "InvalidLedgerIndex",
            R"({
                "ledger_index": "wrong"
            })",
            "invalidParams",
            "ledgerIndexMalformed"
        },

        ParamTestCaseBundle{"UnknownOption", R"({})", "invalidParams", "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthType",
            R"({
                "deposit_preauth": 123
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidDepositPreauthString",
            R"({
                "deposit_preauth": "invalid"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidDepositPreauthEmtpyJson",
            R"({
                "deposit_preauth": {}
            })",
            "invalidParams",
            "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonWrongAccount",
            R"({
                "deposit_preauth": {
                    "owner": "invalid",
                    "authorized": "invalid"
                }
            })",
            "malformedOwner",
            "Malformed owner."
        },

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonOwnerNotString",
            R"({
                "deposit_preauth": {
                    "owner": 123,
                    "authorized": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."
        },

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonAuthorizedNotString",
            fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": 123
                    }}
                }})",
                ACCOUNT
            ),
            "invalidParams",
            "authorizedNotString"
        },

        ParamTestCaseBundle{
            "InvalidTicketType",
            R"({
                "ticket": 123
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidTicketIndex",
            R"({
                "ticket": "invalid"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidTicketEmptyJson",
            R"({
                "ticket": {}
            })",
            "invalidParams",
            "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            "InvalidTicketJsonAccountNotString",
            R"({
                "ticket": {
                    "account": 123,
                    "ticket_seq": 123
                }
            })",
            "invalidParams",
            "accountNotString"
        },

        ParamTestCaseBundle{
            "InvalidTicketJsonAccountInvalid",
            R"({
                "ticket": {
                    "account": "123",
                    "ticket_seq": 123
                }
            })",
            "malformedAddress",
            "Malformed address."
        },

        ParamTestCaseBundle{
            "InvalidTicketJsonSeqNotInt",
            fmt::format(
                R"({{
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidOfferType",
            R"({
                "offer": 123
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidOfferIndex",
            R"({
                "offer": "invalid"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidOfferEmptyJson",
            R"({
                "offer": {}
            })",
            "invalidParams",
            "Required field 'account' missing"
        },

        ParamTestCaseBundle{
            "InvalidOfferJsonAccountNotString",
            R"({
                "ticket": {
                    "account": 123,
                    "seq": 123
                }
            })",
            "invalidParams",
            "accountNotString"
        },

        ParamTestCaseBundle{
            "InvalidOfferJsonAccountInvalid",
            R"({
                "ticket": {
                    "account": "123",
                    "seq": 123
                }
            })",
            "malformedAddress",
            "Malformed address."
        },

        ParamTestCaseBundle{
            "InvalidOfferJsonSeqNotInt",
            fmt::format(
                R"({{
                    "offer": {{
                        "account": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidEscrowType",
            R"({
                "escrow": 123
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidEscrowIndex",
            R"({
                "escrow": "invalid"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidEscrowEmptyJson",
            R"({
                "escrow": {}
            })",
            "invalidParams",
            "Required field 'owner' missing"
        },

        ParamTestCaseBundle{
            "InvalidEscrowJsonAccountNotString",
            R"({
                "escrow": {
                    "owner": 123,
                    "seq": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."
        },

        ParamTestCaseBundle{
            "InvalidEscrowJsonAccountInvalid",
            R"({
                "escrow": {
                    "owner": "123",
                    "seq": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."
        },

        ParamTestCaseBundle{
            "InvalidEscrowJsonSeqNotInt",
            fmt::format(
                R"({{
                    "escrow": {{
                        "owner": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidRippleStateType",
            R"({
                "ripple_state": "123"
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidRippleStateMissField",
            R"({
                "ripple_state": {
                    "currency": "USD"
                }
            })",
            "invalidParams",
            "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateEmtpyJson",
            R"({
                "ripple_state": {}
            })",
            "invalidParams",
            "Required field 'accounts' missing"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateOneAccount",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}"]
                    }}
                }})",
                ACCOUNT
            ),
            "invalidParams",
            "malformedAccounts"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateSameAccounts",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT
            ),
            "invalidParams",
            "malformedAccounts"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateWrongAccountsNotString",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}",123],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT
            ),
            "invalidParams",
            "malformedAccounts"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateWrongAccountsFormat",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","123"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedAddress",
            "malformedAddresses"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateWrongCurrency",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "XXXX"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            "malformedCurrency",
            "malformedCurrency"
        },

        ParamTestCaseBundle{
            "InvalidRippleStateWrongCurrencyNotString",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": 123
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2
            ),
            "invalidParams",
            "currencyNotString"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryType",
            R"({
                "directory": 123
            })",
            "invalidParams",
            "Invalid parameters."
        },

        ParamTestCaseBundle{
            "InvalidDirectoryIndex",
            R"({
                "directory": "123"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidDirectoryEmtpyJson",
            R"({
                "directory": {}
            })",
            "invalidParams",
            "missingOwnerOrDirRoot"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryWrongOwnerNotString",
            R"({
                "directory": {
                    "owner": 123
                }
            })",
            "invalidParams",
            "ownerNotString"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryWrongOwnerFormat",
            R"({
                "directory": {
                    "owner": "123"
                }
            })",
            "malformedAddress",
            "Malformed address."
        },

        ParamTestCaseBundle{
            "InvalidDirectoryWrongDirFormat",
            R"({
                "directory": {
                    "dir_root": "123"
                }
            })",
            "invalidParams",
            "dir_rootMalformed"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryWrongDirNotString",
            R"({
                "directory": {
                    "dir_root": 123
                }
            })",
            "invalidParams",
            "dir_rootNotString"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryDirOwnerConflict",
            fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "owner": "{}"
                    }}
                }})",
                INDEX1,
                ACCOUNT
            ),
            "invalidParams",
            "mayNotSpecifyBothDirRootAndOwner"
        },

        ParamTestCaseBundle{
            "InvalidDirectoryDirSubIndexNotInt",
            fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": "not int"
                    }}
                }})",
                INDEX1
            ),
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "InvalidAMMStringIndex",
            R"({
                "amm": "invalid"
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "EmptyAMMJson",
            R"({
                "amm": {}
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "NonObjectAMMJsonAsset",
            R"({
                "amm": {
                    "asset": 123,
                    "asset2": 123
                }
            })",
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "EmptyAMMAssetJson",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "EmptyAMMAsset2Json",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "MissingAMMAsset2Json",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "MissingAMMAssetJson",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "AMMAssetNotJson",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "AMMAsset2NotJson",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "WrongAMMAssetCurrency",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "WrongAMMAssetIssuer",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "MissingAMMAssetIssuerForNonXRP",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "AMMAssetHasIssuerForXRP",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },

        ParamTestCaseBundle{
            "MissingAMMAssetCurrency",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeMissingBridgeAccount",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeCurrencyIsNumber",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssuerIsNumber",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssuingChainIssueIsNotObject",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeWithInvalidBridgeAccount",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeDoorInvalid",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssuerInvalid",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssueCurrencyInvalid",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssueXRPCurrencyInvalid",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeIssueJPYCurrencyInvalid",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeMissingLockingChainDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeMissingIssuingChainDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeMissingLockingChainIssue",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeMissingIssuingChainIssue",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "BridgeInvalidType",
            fmt::format(
                R"({{
                    "bridge_account": "{}",
                    "bridge": "invalid"
                }})",
                ACCOUNT
            ),
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdInvalidType",
            R"({
                "xchain_owned_claim_id": 123
            })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdJsonMissingClaimId",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdJsonMissingDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdJsonMissingIssue",
            fmt::format(
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

            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdJsonInvalidDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedClaimIdJsonInvalidIssue",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdInvalidType",
            R"({
                    "xchain_owned_create_account_claim_id": 123
                    })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdJsonMissingClaimId",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdJsonMissingDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdJsonMissingIssue",
            fmt::format(
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

            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdJsonInvalidDoor",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OwnedCreateAccountClaimIdJsonInvalidIssue",
            fmt::format(
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
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdMissing",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidNegative",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": -1
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidTypeString",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": "invalid"
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidTypeDouble",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 3.21
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidTypeObject",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": {{}}
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidTypeArray",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": []
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectDocumentIdInvalidTypeNull",
            fmt::format(
                R"({{
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": null
                    }}
                }})",
                ACCOUNT
            ),
            "malformedDocumentID",
            "Malformed oracle_document_id."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountMissing",
            R"({
                "oracle": {
                    "oracle_document_id": 1
                }
            })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidTypeInteger",
            R"({
                "oracle": {
                    "account": 123,
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidTypeDouble",
            R"({
                "oracle": {
                    "account": 123.45,
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidTypeNull",
            R"({
                "oracle": {
                    "account": null,
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidTypeObject",
            R"({
                "oracle": {
                    "account": {"test": "test"},
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidTypeArray",
            R"({
                "oracle": {
                    "account": [{"test": "test"}],
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleObjectAccountInvalidFormat",
            R"({
                "oracle": {
                    "account": "NotHex",
                    "oracle_document_id": 1
                }
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleStringInvalidFormat",
            R"({
                "oracle": "NotHex"
            })",
            "malformedAddress",
            "Malformed address."
        },
        ParamTestCaseBundle{
            "OracleStringInvalidTypeInteger",
            R"({
                "oracle": 123
            })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleStringInvalidTypeDouble",
            R"({
                "oracle": 123.45
            })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleStringInvalidTypeArray",
            R"({
                "oracle": [{"test": "test"}]
            })",
            "malformedRequest",
            "Malformed request."
        },
        ParamTestCaseBundle{
            "OracleStringInvalidTypeNull",
            R"({
                "oracle": null
            })",
            "malformedRequest",
            "Malformed request."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup1,
    LedgerEntryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    LedgerEntryParameterTest::NameGenerator{}
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
    backend->setRange(RANGEMIN, RANGEMAX);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

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

struct RPCLedgerEntryNormalPathTest : public RPCLedgerEntryTest, public WithParamInterface<NormalPathTestBundle> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<NormalPathTestBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForNormalPathTest()
{
    auto account1 = GetAccountIDWithString(ACCOUNT);
    auto account2 = GetAccountIDWithString(ACCOUNT2);
    ripple::Currency currency;
    ripple::to_currency(currency, "USD");

    return std::vector<NormalPathTestBundle>{
        NormalPathTestBundle{
            "Index",
            fmt::format(
                R"({{
                    "binary": true,
                    "index": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateAccountRootObject(ACCOUNT2, ripple::lsfGlobalFreeze, 1, 10, 2, INDEX1, 3)
        },
        NormalPathTestBundle{
            "Payment_channel",
            fmt::format(
                R"({{
                    "binary": true,
                    "payment_channel": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400)
        },
        NormalPathTestBundle{
            "Nft_page",
            fmt::format(
                R"({{
                    "binary": true,
                    "nft_page": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateNFTTokenPage(
                std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt
            )
        },
        NormalPathTestBundle{
            "Check",
            fmt::format(
                R"({{
                    "binary": true,
                    "check": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateCheckLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            "DirectoryIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            "OfferIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "offer": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1
            )
        },
        NormalPathTestBundle{
            "EscrowIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "escrow": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            "TicketIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "ticket": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateTicketLedgerObject(ACCOUNT, 0)
        },
        NormalPathTestBundle{
            "DepositPreauthIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "deposit_preauth": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateDepositPreauthLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            "AccountRoot",
            fmt::format(
                R"({{
                    "binary": true,
                    "account_root": "{}"
                }})",
                ACCOUNT
            ),
            ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key,
            CreateAccountRootObject(ACCOUNT, 0, 1, 1, 1, INDEX1, 1)
        },
        NormalPathTestBundle{
            "DID",
            fmt::format(
                R"({{
                    "binary": true,
                    "did": "{}"
                }})",
                ACCOUNT
            ),
            ripple::keylet::did(GetAccountIDWithString(ACCOUNT)).key,
            CreateDidObject(ACCOUNT, "mydocument", "myURI", "mydata")
        },
        NormalPathTestBundle{
            "DirectoryViaDirRoot",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": 2
                    }}
                }})",
                INDEX1
            ),
            ripple::keylet::page(ripple::uint256{INDEX1}, 2).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            "DirectoryViaOwner",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}",
                        "sub_index": 2
                    }}
                }})",
                ACCOUNT
            ),
            ripple::keylet::page(ripple::keylet::ownerDir(account1), 2).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            "DirectoryViaDefaultSubIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}"
                    }}
                }})",
                ACCOUNT
            ),
            // default sub_index is 0
            ripple::keylet::page(ripple::keylet::ownerDir(account1), 0).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)
        },
        NormalPathTestBundle{
            "Escrow",
            fmt::format(
                R"({{
                    "binary": true,
                    "escrow": {{
                        "owner": "{}",
                        "seq": 1
                    }}
                }})",
                ACCOUNT
            ),
            ripple::keylet::escrow(account1, 1).key,
            CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            "DepositPreauth",
            fmt::format(
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
            ripple::keylet::depositPreauth(account1, account2).key,
            CreateDepositPreauthLedgerObject(ACCOUNT, ACCOUNT2)
        },
        NormalPathTestBundle{
            "RippleState",
            fmt::format(
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
            ripple::keylet::line(account1, account2, currency).key,
            CreateRippleStateLedgerObject("USD", ACCOUNT2, 100, ACCOUNT, 10, ACCOUNT2, 20, INDEX1, 123, 0)
        },
        NormalPathTestBundle{
            "Ticket",
            fmt::format(
                R"({{
                    "binary": true,
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": 2
                    }}
                }})",
                ACCOUNT
            ),
            ripple::getTicketIndex(account1, 2),
            CreateTicketLedgerObject(ACCOUNT, 0)
        },
        NormalPathTestBundle{
            "Offer",
            fmt::format(
                R"({{
                    "binary": true,
                    "offer": {{
                        "account": "{}",
                        "seq": 2
                    }}
                }})",
                ACCOUNT
            ),
            ripple::keylet::offer(account1, 2).key,
            CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1
            )
        },
        NormalPathTestBundle{
            "AMMViaIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "amm": "{}"
                }})",
                INDEX1
            ),
            ripple::uint256{INDEX1},
            CreateAMMObject(ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", ACCOUNT2)
        },
        NormalPathTestBundle{
            "AMMViaJson",
            fmt::format(
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
            ripple::keylet::amm(GetIssue("XRP", ripple::toBase58(ripple::xrpAccount())), GetIssue("JPY", ACCOUNT2)).key,
            CreateAMMObject(ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", ACCOUNT2)
        },
        NormalPathTestBundle{
            "BridgeLocking",
            fmt::format(
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
            ripple::keylet::bridge(
                ripple::STXChainBridge(
                    GetAccountIDWithString(ACCOUNT),
                    ripple::xrpIssue(),
                    GetAccountIDWithString(ACCOUNT2),
                    GetIssue("JPY", ACCOUNT3)
                ),
                ripple::STXChainBridge::ChainType::locking
            )
                .key,
            CreateBridgeObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3)
        },
        NormalPathTestBundle{
            "BridgeIssuing",
            fmt::format(
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
            ripple::keylet::bridge(
                ripple::STXChainBridge(
                    GetAccountIDWithString(ACCOUNT),
                    ripple::xrpIssue(),
                    GetAccountIDWithString(ACCOUNT2),
                    GetIssue("JPY", ACCOUNT3)
                ),
                ripple::STXChainBridge::ChainType::issuing
            )
                .key,
            CreateBridgeObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3)
        },
        NormalPathTestBundle{
            "XChainOwnedClaimId",
            fmt::format(
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
            ripple::keylet::xChainClaimID(
                ripple::STXChainBridge(
                    GetAccountIDWithString(ACCOUNT),
                    ripple::xrpIssue(),
                    GetAccountIDWithString(ACCOUNT2),
                    GetIssue("JPY", ACCOUNT3)
                ),
                10
            )
                .key,
            CreateChainOwnedClaimIDObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3, ACCOUNT)
        },
        NormalPathTestBundle{
            "XChainOwnedCreateAccountClaimId",
            fmt::format(
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
            ripple::keylet::xChainCreateAccountClaimID(
                ripple::STXChainBridge(
                    GetAccountIDWithString(ACCOUNT),
                    ripple::xrpIssue(),
                    GetAccountIDWithString(ACCOUNT2),
                    GetIssue("JPY", ACCOUNT3)
                ),
                10
            )
                .key,
            CreateChainOwnedClaimIDObject(ACCOUNT, ACCOUNT, ACCOUNT2, "JPY", ACCOUNT3, ACCOUNT)
        },
        NormalPathTestBundle{
            "OracleEntryFoundViaObject",
            fmt::format(
                R"({{
                    "binary": true,
                    "oracle": {{
                        "account": "{}",
                        "oracle_document_id": 1
                    }}
                }})",
                ACCOUNT
            ),
            ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key,
            CreateOracleObject(
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
            "OracleEntryFoundViaString",
            fmt::format(
                R"({{
                    "binary": true,
                    "oracle": "{}"
                }})",
                ripple::to_string(ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key)
            ),
            ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), 1).key,
            CreateOracleObject(
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
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup2,
    RPCLedgerEntryNormalPathTest,
    ValuesIn(generateTestValuesForNormalPathTest()),
    RPCLedgerEntryNormalPathTest::NameGenerator{}
);

// Test for normal path
// Check the index in response matches the computed index accordingly
TEST_P(RPCLedgerEntryNormalPathTest, NormalPath)
{
    auto const testBundle = GetParam();

    backend->setRange(RANGEMIN, RANGEMAX);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

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

    backend->setRange(RANGEMIN, RANGEMAX);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

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
    backend->setRange(RANGEMIN, RANGEMAX);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillRepeatedly(Return(ledgerinfo));

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
    backend->setRange(RANGEMIN, RANGEMAX);

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
    backend->setRange(RANGEMIN, RANGEMAX);

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
    backend->setRange(RANGEMIN, RANGEMAX);

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
    EXPECT_EQ(warning.at("id").as_uint64(), static_cast<uint64_t>(rpc::WarningCode::warnRPC_DEPRECATED));
    EXPECT_NE(warning.at("message").as_string().find("Field 'ledger' is deprecated."), std::string::npos) << warning;
}
