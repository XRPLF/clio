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
#include <handlers/RPCHelpers.h>
#include <optional>

boost::json::object
doChannelVerify(boost::json::object const& request)
{
    boost::json::object response;
    if(!request.contains("channel_id"))
    {
        response["error"] = "missing field channel_id";
        return response;
    }

    if(!request.contains("amount"))
    {
        response["error"] = "missing field amount";
        return response;
    }

    if (!request.contains("signature"))
    {
        response["error"] = "missing field signature";
        return response;
    }
    
    if (!request.contains("public_key"))
    {
        response["error"] = "missing field public_key";
        return response;
    }

    boost::optional<ripple::PublicKey> pk;
    {
        std::string const strPk = request.at("public_key").as_string().c_str();
        pk = ripple::parseBase58<ripple::PublicKey>(ripple::TokenType::AccountPublic, strPk);

        if (!pk)
        {
            auto pkHex = ripple::strUnHex(strPk);
            if (!pkHex)
            {
                response["error"] = "malformed public key";
                return response;
            }
            auto const pkType = ripple::publicKeyType(ripple::makeSlice(*pkHex));
            if (!pkType)
            {
                response["error"] = "invalid key type";
            }

            pk.emplace(ripple::makeSlice(*pkHex));
        }
    }

    ripple::uint256 channelId;
    if (!channelId.parseHex(request.at("channel_id").as_string().c_str()))
    {
        response["error"] = "channel id malformed";
        return response;
    }

    auto optDrops =
        ripple::to_uint64(request.at("amount").as_string().c_str());

    if (!optDrops)
    {
        response["error"] = "could not parse channel amount";
        return response;
    }

    std::uint64_t drops = *optDrops;

    if (!request.at("signature").is_string())
    {
        response["error"] = "signature must be type string";
        return response;
    }

    auto sig = ripple::strUnHex(request.at("signature").as_string().c_str());
    
    if (!sig || !sig->size())
    {
        response["error"] = "Invalid signature";
        return response;
    }

    ripple::Serializer msg;
    ripple::serializePayChanAuthorization(msg, channelId, ripple::XRPAmount(drops));

    response["signature_verified"] =
        ripple::verify(*pk, msg.slice(), ripple::makeSlice(*sig), true);
    
    return response;
}