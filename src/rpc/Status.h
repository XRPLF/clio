//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2021 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RPC_ERRORCODES_H_INCLUDED
#define RPC_ERRORCODES_H_INCLUDED

#include <string>
#include <boost/json.hpp>

namespace RPC
{

// RPC Error codes from rippled
// NOTE: Although the precise numeric values of these codes were never
// intended to be stable, several API endpoints include the numeric values.
// Some users came to rely on the values, meaning that renumbering would be
// a breaking change for those users.
//
// We therefore treat the range of values as stable although they are not
// and are subject to change.
//
// Please only append to this table. Do not "fill-in" gaps and do not re-use
// or repurpose error code values.
enum class Error {
    // -1 represents codes not listed in this enumeration
    rpcUNKNOWN = -1,

    rpcSUCCESS = 0,

    rpcBAD_SYNTAX = 1,
    rpcJSON_RPC = 2,
    rpcFORBIDDEN = 3,

    // Misc failure
    // unused                  4,
    // unused                  5,
    rpcNO_PERMISSION = 6,
    rpcNO_EVENTS = 7,
    // unused                  8,
    rpcTOO_BUSY = 9,
    rpcSLOW_DOWN = 10,
    rpcHIGH_FEE = 11,
    rpcNOT_ENABLED = 12,
    rpcNOT_READY = 13,
    rpcAMENDMENT_BLOCKED = 14,

    // Networking
    rpcNO_CLOSED = 15,
    rpcNO_CURRENT = 16,
    rpcNO_NETWORK = 17,
    rpcNOT_SYNCED = 18,

    // Ledger state
    rpcACT_NOT_FOUND = 19,
    // unused                  20,
    rpcLGR_NOT_FOUND = 21,
    rpcLGR_NOT_VALIDATED = 22,
    rpcMASTER_DISABLED = 23,
    // unused                  24,
    // unused                  25,
    // unused                  26,
    // unused                  27,
    // unused                  28,
    rpcTXN_NOT_FOUND = 29,
    // unused                  30,

    // Malformed command
    rpcINVALID_PARAMS = 31,
    rpcUNKNOWN_COMMAND = 32,
    rpcNO_PF_REQUEST = 33,

    // Bad parameter
    // NOT USED DO NOT USE AGAIN rpcACT_BITCOIN = 34,
    rpcACT_MALFORMED = 35,
    rpcALREADY_MULTISIG = 36,
    rpcALREADY_SINGLE_SIG = 37,
    // unused                  38,
    // unused                  39,
    rpcBAD_FEATURE = 40,
    rpcBAD_ISSUER = 41,
    rpcBAD_MARKET = 42,
    rpcBAD_SECRET = 43,
    rpcBAD_SEED = 44,
    rpcCHANNEL_MALFORMED = 45,
    rpcCHANNEL_AMT_MALFORMED = 46,
    rpcCOMMAND_MISSING = 47,
    rpcDST_ACT_MALFORMED = 48,
    rpcDST_ACT_MISSING = 49,
    rpcDST_ACT_NOT_FOUND = 50,
    rpcDST_AMT_MALFORMED = 51,
    rpcDST_AMT_MISSING = 52,
    rpcDST_ISR_MALFORMED = 53,
    // unused                  54,
    // unused                  55,
    // unused                  56,
    rpcLGR_IDXS_INVALID = 57,
    rpcLGR_IDX_MALFORMED = 58,
    // unused                  59,
    // unused                  60,
    // unused                  61,
    rpcPUBLIC_MALFORMED = 62,
    rpcSIGNING_MALFORMED = 63,
    rpcSENDMAX_MALFORMED = 64,
    rpcSRC_ACT_MALFORMED = 65,
    rpcSRC_ACT_MISSING = 66,
    rpcSRC_ACT_NOT_FOUND = 67,
    // unused                  68,
    rpcSRC_CUR_MALFORMED = 69,
    rpcSRC_ISR_MALFORMED = 70,
    rpcSTREAM_MALFORMED = 71,
    rpcATX_DEPRECATED = 72,

    // Internal error (should never happen)
    rpcINTERNAL = 73,  // Generic internal error.
    rpcNOT_IMPL = 74,
    rpcNOT_SUPPORTED = 75,
    rpcBAD_KEY_TYPE = 76,
    rpcDB_DESERIALIZATION = 77,
    rpcEXCESSIVE_LGR_RANGE = 78,
    rpcINVALID_LGR_RANGE = 79,
    rpcEXPIRED_VALIDATOR_LIST = 80,

    // Reporting
    rpcFAILED_TO_FORWARD = 90,
    rpcREPORTING_UNSUPPORTED = 91,
    rpcENTRY_NOT_FOUND = 92,
    rpcLAST =
        rpcENTRY_NOT_FOUND  // rpcLAST should always equal the last code.=
};

/** Codes returned in the `warnings` array of certain RPC commands.

    These values need to remain stable.
*/
enum class Warning {
    warnRPC_UNSUPPORTED_MAJORITY = 1001,
    warnRPC_AMENDMENT_BLOCKED = 1002,
    warnRPC_EXPIRED_VALIDATOR_LIST = 1003,
    warnRPC_REPORTING = 1004
};

struct ErrorInfo
{
    ErrorInfo() {};

    ErrorInfo(
        char const* token_,
        char const* message_)
    : token(token_)
    , message(message_) 
    {}

    std::string token = "";
    std::string message = "";
};

struct Status
{
    Error error = Error::rpcSUCCESS;
    std::string message = "";

    Status() {};

    Status(Error error_) : error(error_) {};

    Status(Error error_, std::string message_)
    : error(error_)
    , message(message_) 
    {}

    /** Returns true if the Status is *not* OK. */
    operator bool() const
    {
        return error != Error::rpcSUCCESS;
    }
};

static Status OK;

ErrorInfo const&
get_error_info(Error code);

void
inject_error(Error err, boost::json::object& json);

void
inject_error(Error err, std::string const& message, boost::json::object& json);

boost::json::object
make_error(Error err);

boost::json::object
make_error(Error err, std::string const& message);

} // namespace RPC

#endif // RPC_ERRORCODES_H_INCLUDED