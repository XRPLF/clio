#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/PayChan.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/jss.h>
#include <ripple/resource/Fees.h>

#include <optional>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doChannelVerify(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("channel_id"))
        return Status{Error::rpcINVALID_PARAMS, "missingChannelID"};

    if (!request.at("channel_id").is_string())
        return Status{Error::rpcINVALID_PARAMS, "channelIDNotString"};

    if (!request.contains("amount"))
        return Status{Error::rpcINVALID_PARAMS, "missingAmount"};

    if (!request.at("amount").is_string())
        return Status{Error::rpcINVALID_PARAMS, "amountNotString"};

    if (!request.contains("signature"))
        return Status{Error::rpcINVALID_PARAMS, "missingSignature"};

    if (!request.at("signature").is_string())
        return Status{Error::rpcINVALID_PARAMS, "signatureNotString"};

    if (!request.contains("public_key"))
        return Status{Error::rpcINVALID_PARAMS, "missingPublicKey"};

    if (!request.at("public_key").is_string())
        return Status{Error::rpcINVALID_PARAMS, "publicKeyNotString"};

    std::optional<ripple::PublicKey> pk;
    {
        std::string const strPk = request.at("public_key").as_string().c_str();
        pk = ripple::parseBase58<ripple::PublicKey>(
            ripple::TokenType::AccountPublic, strPk);

        if (!pk)
        {
            auto pkHex = ripple::strUnHex(strPk);
            if (!pkHex)
                return Status{Error::rpcPUBLIC_MALFORMED, "malformedPublicKey"};

            auto const pkType =
                ripple::publicKeyType(ripple::makeSlice(*pkHex));
            if (!pkType)
                return Status{Error::rpcPUBLIC_MALFORMED, "invalidKeyType"};

            pk.emplace(ripple::makeSlice(*pkHex));
        }
    }

    ripple::uint256 channelId;
    if (!channelId.parseHex(request.at("channel_id").as_string().c_str()))
        return Status{Error::rpcCHANNEL_MALFORMED, "malformedChannelID"};

    auto optDrops = ripple::to_uint64(request.at("amount").as_string().c_str());

    if (!optDrops)
        return Status{Error::rpcCHANNEL_AMT_MALFORMED, "couldNotParseAmount"};

    std::uint64_t drops = *optDrops;

    auto sig = ripple::strUnHex(request.at("signature").as_string().c_str());

    if (!sig || !sig->size())
        return Status{Error::rpcINVALID_PARAMS, "invalidSignature"};

    ripple::Serializer msg;
    ripple::serializePayChanAuthorization(
        msg, channelId, ripple::XRPAmount(drops));

    response["signature_verified"] =
        ripple::verify(*pk, msg.slice(), ripple::makeSlice(*sig), true);

    return response;
}

}  // namespace RPC
