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

#include <handlers/Status.h>
#include <unordered_map>
#include <variant>

namespace RPC
{

static std::unordered_map<Error, ErrorInfo> errorTable {
    {Error::rpcACT_MALFORMED, {"actMalformed", "Account malformed."}},
    {Error::rpcACT_NOT_FOUND, {"actNotFound", "Account not found."}},
    {Error::rpcALREADY_MULTISIG, {"alreadyMultisig", "Already multisigned."}},
    {Error::rpcALREADY_SINGLE_SIG, {"alreadySingleSig", "Already single-signed."}},
    {Error::rpcATX_DEPRECATED,
    {"deprecated",
    "Use the new API or specify a ledger range."}},
    {Error::rpcBAD_KEY_TYPE, {"badKeyType", "Bad key type."}},
    {Error::rpcBAD_FEATURE, {"badFeature", "Feature unknown or invalid."}},
    {Error::rpcBAD_ISSUER, {"badIssuer", "Issuer account malformed."}},
    {Error::rpcBAD_MARKET, {"badMarket", "No such market."}},
    {Error::rpcBAD_SECRET, {"badSecret", "Secret does not match account."}},
    {Error::rpcBAD_SEED, {"badSeed", "Disallowed seed."}},
    {Error::rpcBAD_SYNTAX, {"badSyntax", "Syntax error."}},
    {Error::rpcCHANNEL_MALFORMED, {"channelMalformed", "Payment channel is malformed."}},
    {Error::rpcCHANNEL_AMT_MALFORMED,
    {"channelAmtMalformed",
    "Payment channel amount is malformed."}},
    {Error::rpcBAD_MARKET, {"badMarket", "No such market."}},
    {Error::rpcBAD_SECRET, {"badSecret", "Secret does not match account."}},
    {Error::rpcBAD_SEED, {"badSeed", "Disallowed seed."}},
    {Error::rpcBAD_SYNTAX, {"badSyntax", "Syntax error."}},
    {Error::rpcCHANNEL_MALFORMED, {"channelMalformed", "Payment channel is malformed."}},
    {Error::rpcCHANNEL_AMT_MALFORMED,
        {"channelAmtMalformed",
        "Payment channel amount is malformed."}},
    {Error::rpcCOMMAND_MISSING, {"commandMissing", "Missing command entry."}},
    {Error::rpcDB_DESERIALIZATION,
        {"dbDeserialization",
        "Database deserialization error."}},
    {Error::rpcDST_ACT_MALFORMED,
        {"dstActMalformed",
        "Destination account is malformed."}},
    {Error::rpcDST_ACT_MISSING, {"dstActMissing", "Destination account not provided."}},
    {Error::rpcDST_ACT_NOT_FOUND, {"dstActNotFound", "Destination account not found."}},
    {Error::rpcDST_AMT_MALFORMED,
        {"dstAmtMalformed",
        "Destination amount/currency/issuer is malformed."}},
    {Error::rpcDST_AMT_MISSING,
        {"dstAmtMissing",
        "Destination amount/currency/issuer is missing."}},
    {Error::rpcDST_ISR_MALFORMED,
        {"dstIsrMalformed",
        "Destination issuer is malformed."}},
    {Error::rpcEXCESSIVE_LGR_RANGE, {"excessiveLgrRange", "Ledger range exceeds 1000."}},
    {Error::rpcFORBIDDEN, {"forbidden", "Bad credentials."}},
    {Error::rpcFAILED_TO_FORWARD,
        {"failedToForward",
        "Failed to forward request to p2p node"}},
    {Error::rpcHIGH_FEE, {"highFee", "Current transaction fee exceeds your limit."}},
    {Error::rpcINTERNAL, {"internal", "Internal error."}},
    {Error::rpcINVALID_LGR_RANGE, {"invalidLgrRange", "Ledger range is invalid."}},
    {Error::rpcINVALID_PARAMS, {"invalidParams", "Invalid parameters."}},
    {Error::rpcJSON_RPC, {"json_rpc", "JSON-RPC transport error."}},
    {Error::rpcLGR_IDXS_INVALID, {"lgrIdxsInvalid", "Ledger indexes invalid."}},
    {Error::rpcLGR_IDX_MALFORMED, {"lgrIdxMalformed", "Ledger index malformed."}},
    {Error::rpcLGR_NOT_FOUND, {"lgrNotFound", "Ledger not found."}},
    {Error::rpcLGR_NOT_VALIDATED, {"lgrNotValidated", "Ledger not validated."}},
    {Error::rpcMASTER_DISABLED, {"masterDisabled", "Master key is disabled."}},
    {Error::rpcNOT_ENABLED, {"notEnabled", "Not enabled in configuration."}},
    {Error::rpcNOT_IMPL, {"notImpl", "Not implemented."}},
    {Error::rpcNOT_READY, {"notReady", "Not ready to handle this request."}},
    {Error::rpcNOT_SUPPORTED, {"notSupported", "Operation not supported."}},
    {Error::rpcNO_CLOSED, {"noClosed", "Closed ledger is unavailable."}},
    {Error::rpcNO_CURRENT, {"noCurrent", "Current ledger is unavailable."}},
    {Error::rpcNOT_SYNCED, {"notSynced", "Not synced to the network."}},
    {Error::rpcNO_EVENTS, {"noEvents", "Current transport does not support events."}},
    {Error::rpcNO_NETWORK, {"noNetwork", "Not synced to the network."}},
    {Error::rpcNO_PERMISSION,
        {"noPermission",
        "You don't have permission for this command."}},
    {Error::rpcNO_PF_REQUEST, {"noPathRequest", "No pathfinding request in progress."}},
    {Error::rpcPUBLIC_MALFORMED, {"publicMalformed", "Public key is malformed."}},
    {Error::rpcREPORTING_UNSUPPORTED,
        {"reportingUnsupported",
        "Requested operation not supported by reporting mode server"}},
    {Error::rpcSIGNING_MALFORMED,
        {"signingMalformed",
        "Signing of transaction is malformed."}},
    {Error::rpcSLOW_DOWN, {"slowDown", "You are placing too much load on the server."}},
    {Error::rpcSRC_ACT_MALFORMED, {"srcActMalformed", "Source account is malformed."}},
    {Error::rpcSRC_ACT_MISSING, {"srcActMissing", "Source account not provided."}},
    {Error::rpcSRC_ACT_NOT_FOUND, {"srcActNotFound", "Source account not found."}},
    {Error::rpcSRC_CUR_MALFORMED, {"srcCurMalformed", "Source currency is malformed."}},
    {Error::rpcSRC_ISR_MALFORMED, {"srcIsrMalformed", "Source issuer is malformed."}},
    {Error::rpcSTREAM_MALFORMED, {"malformedStream", "Stream malformed."}},
    {Error::rpcTOO_BUSY, {"tooBusy", "The server is too busy to help you now."}},
    {Error::rpcTXN_NOT_FOUND, {"txnNotFound", "Transaction not found."}},
    {Error::rpcUNKNOWN_COMMAND, {"unknownCmd", "Unknown method."}},
    {Error::rpcSENDMAX_MALFORMED, {"sendMaxMalformed", "SendMax amount malformed."}},
    {Error::rpcINVALID_PARAMS, {"malformedRequest", "Request malformed"}},
    {Error::rpcENTRY_NOT_FOUND, {"entryNotFound", "ledger entry not found"}}
};

ErrorInfo const&
get_error_info(Error code)
{
    return errorTable[code];
}

void
inject_error(Error err, boost::json::object& json)
{
    ErrorInfo const& info(get_error_info(err));
    json["error"] = info.token;
    json["error_code"] = static_cast<std::uint32_t>(err);
    json["error_message"] = info.message;
    json["status"] = "error";
    json["type"] = "response";
}

void
inject_error(Error err, std::string const& message, boost::json::object& json)
{
    ErrorInfo const& info(get_error_info(err));
    json["error"] = info.token;
    json["error_code"] = static_cast<std::uint32_t>(err);
    json["error_message"] = message;
    json["status"] = "error";
    json["type"] = "response";
}

boost::json::object
make_error(Error err)
{
    boost::json::object json{};
    inject_error(err, json);
    return json;
}

boost::json::object
make_error(Error err, std::string const& message)
{
    boost::json::object json{};
    inject_error(err, message, json);
    return json;
}
}