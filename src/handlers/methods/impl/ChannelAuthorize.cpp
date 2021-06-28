//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

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

#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/PayChan.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/jss.h>
#include <ripple/resource/Fees.h>
#include <handlers/methods/Channel.h>
#include <handlers/RPCHelpers.h>
#include <optional>

namespace RPC
{

void
serializePayChanAuthorization(
    ripple::Serializer& msg,
    ripple::uint256 const& key,
    ripple::XRPAmount const& amt)
{
    msg.add32(ripple::HashPrefix::paymentChannelClaim);
    msg.addBitString(key);
    msg.add64(amt.drops());
}

Status
ChannelAuthorize::check()
{
    auto request = context_.params;

    if(!request.contains("channel_id"))
        return {Error::rpcINVALID_PARAMS, "missingChannelID"};
    
    if(!request.at("channel_id").is_string())
        return {Error::rpcINVALID_PARAMS, "channelIDNotString"};

    if(!request.contains("amount"))
        return {Error::rpcINVALID_PARAMS, "missingAmount"};

    if(!request.at("amount").is_string())
            return {Error::rpcINVALID_PARAMS, "amountNotString"};

    if (!request.contains("key_type") && !request.contains("secret"))
        return {Error::rpcINVALID_PARAMS, "missingKeyTypeOrSecret"};

    auto v = keypairFromRequst(request);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto const [pk, sk] = std::get<std::pair<ripple::PublicKey, ripple::SecretKey>>(v);

    ripple::uint256 channelId;
    if (!channelId.parseHex(request.at("channel_id").as_string().c_str()))
        return {Error::rpcCHANNEL_MALFORMED, "malformedChannelID"};

    auto optDrops =
        ripple::to_uint64(request.at("amount").as_string().c_str());

    if (!optDrops)
        return {Error::rpcCHANNEL_AMT_MALFORMED, "couldNotParseAmount"};

    std::uint64_t drops = *optDrops;

    ripple::Serializer msg;
    ripple::serializePayChanAuthorization(msg, channelId, ripple::XRPAmount(drops));

    try
    {
        auto const buf = ripple::sign(pk, sk, msg.slice());
        response_["signature"] = ripple::strHex(buf);
    }
    catch (std::exception&)
    {
        return {Error::rpcINTERNAL};
    }

    return OK;
}   

} // namesace RPC