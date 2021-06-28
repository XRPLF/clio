#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/handlers/Account.h>
#include <rpc/RPCHelpers.h>
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>
#include <reporting/Pg.h>

namespace RPC
{

void
addChannel(boost::json::array& jsonLines, ripple::SLE const& line)
{
    boost::json::object jDst;
    jDst["channel_id"] = ripple::to_string(line.key());
    jDst["account"] = ripple::to_string(line.getAccountID(ripple::sfAccount));
    jDst["destination_account"] = ripple::to_string(line.getAccountID(ripple::sfDestination));
    jDst["amount"] = line[ripple::sfAmount].getText();
    jDst["balance"] = line[ripple::sfBalance].getText();
    if (publicKeyType(line[ripple::sfPublicKey]))
    {
        ripple::PublicKey const pk(line[ripple::sfPublicKey]);
        jDst["public_key"] = toBase58(ripple::TokenType::AccountPublic, pk);
        jDst["public_key_hex"] = strHex(pk);
    }
    jDst["settle_delay"] = line[ripple::sfSettleDelay];
    if (auto const& v = line[~ripple::sfExpiration])
        jDst["expiration"] = *v;
    if (auto const& v = line[~ripple::sfCancelAfter])
        jDst["cancel_after"] = *v;
    if (auto const& v = line[~ripple::sfSourceTag])
        jDst["source_tag"] = *v;
    if (auto const& v = line[~ripple::sfDestinationTag])
        jDst["destination_tag"] = *v;

    jsonLines.push_back(jDst);
}

Status
AccountChannels::check()
{
    auto request = context_.params;

    auto v = ledgerInfoFromRequest(context_);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    if(!request.contains("account"))
        return {Error::rpcINVALID_PARAMS, "missingAccount"};

    if(!request.at("account").is_string())
        return {Error::rpcINVALID_PARAMS, "accountNotString"};
    
    auto accountID = 
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return {Error::rpcINVALID_PARAMS, "malformedAccount"};

    std::optional<ripple::AccountID> destAccount = {};
    if (request.contains("destination_account"))
    {
        if (!request.at("destination_account").is_string())
            return {Error::rpcINVALID_PARAMS, "destinationNotString"};

        destAccount = accountFromStringStrict(
            request.at("destination_account").as_string().c_str());

        if (!destAccount)
            return {Error::rpcINVALID_PARAMS, "destinationMalformed"};
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return {Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return {Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    ripple::uint256 cursor;
    if (request.contains("cursor"))
    {
        if(!request.at("cursor").is_string())
            return {Error::rpcINVALID_PARAMS, "cursorNotString"};

        if (!cursor.parseHex(request.at("cursor").as_string().c_str()))
            return {Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    response_["account"] = ripple::to_string(*accountID);
    response_["channels"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonChannels = response_.at("channels").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltPAYCHAN &&
            sle.getAccountID(ripple::sfAccount) == *accountID &&
            (!destAccount ||
                *destAccount == sle.getAccountID(ripple::sfDestination)))
        {
            if (limit-- == 0)
            {
                return false;
            }
            
            addChannel(jsonChannels, sle);
        }

        return true;
    };

    auto nextCursor = 
        traverseOwnedNodes(
            *context_.backend,
            *accountID,
            lgrInfo.seq,
            cursor,
            addToResponse);

    response_["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response_["ledger_index"] = lgrInfo.seq;
    if (nextCursor)
        response_["marker"] = ripple::strHex(*nextCursor);

    return OK;
}

} // namespace RPC