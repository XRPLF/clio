#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>
#include <reporting/Pg.h>

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

    jsonLines.push_back(Json::objectValue);
}

boost::json::object
doAccountChannels(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;

    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }

    if(!request.contains("account"))
    {
        response["error"] = "Must contain account";
        return response;
    }

    if(!request.at("account").is_string())
    {
        response["error"] = "Account must be a string";
        return response;
    }
    
    ripple::AccountID accountID;
    auto parsed = ripple::parseBase58<ripple::AccountID>(
            request.at("account").as_string().c_str());

    if (!parsed)
    {
        response["error"] = "Invalid account";
        return response;
    }

    accountID = *parsed;

    boost::optional<ripple::AccountID> destAccount;
    if (request.contains("destination_account"))
    {
        if (!request.at("destination_account").is_string())
        {
            response["error"] = "destination_account should be a string";
            return response;
        }

        destAccount = ripple::parseBase58<ripple::AccountID>(
            request.at("destination_account").as_string().c_str());
        if (!destAccount)
        {
            response["error"] = "Invalid destination account";
            return response;
        }
    }

    auto const rootIndex = ripple::keylet::ownerDir(accountID);
    auto currentIndex = rootIndex;

    std::vector<ripple::uint256> keys;

    for (;;)
    {
        auto ownedNode =
            backend.fetchLedgerObject(currentIndex.key, *ledgerSequence);

        ripple::SerialIter it{ownedNode->data(), ownedNode->size()};
        ripple::SLE dir{it, currentIndex.key};
        for (auto const& key : dir.getFieldV256(ripple::sfIndexes))
            keys.push_back(key);

        auto const uNodeNext = dir.getFieldU64(ripple::sfIndexNext);
        if (uNodeNext == 0)
            break;

        currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
    }

    auto objects = backend.fetchLedgerObjects(keys, *ledgerSequence);

    response["channels"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonChannels = response.at("channels").as_array();

    for (auto i = 0; i < objects.size(); ++i)
    {
        ripple::SerialIter it{objects[i].data(), objects[i].size()};
        ripple::SLE sle{it, keys[i]};

        if (sle.getType() == ripple::ltPAYCHAN &&
            sle.getAccountID(ripple::sfAccount) == accountID &&
            (!destAccount ||
                *destAccount == sle.getAccountID(ripple::sfDestination)))
        {
            addChannel(jsonChannels, sle);
        }
    }

    return response;
}
