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

#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>
#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>

// {
//   account: <ident>,
//   strict: <bool>        // optional (default false)
//                         //   if true only allow public keys and addresses.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   signer_lists : <bool> // optional (default false)
//                         //   if true return SignerList(s).
//   queue : <bool>        // optional (default false)
//                         //   if true return information about transactions
//                         //   in the current TxQ, only if the requested
//                         //   ledger is open. Otherwise if true, returns an
//                         //   error.
// }

// TODO(tom): what is that "default"?
boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;
    std::string strIdent;
    if (request.contains("account"))
        strIdent = request.at("account").as_string().c_str();
    else if (request.contains("ident"))
        strIdent = request.at("ident").as_string().c_str();
    else
    {
        response["error"] = "missing account field";
        return response;
    }
    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;
    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }

    // bool bStrict = request.contains("strict") &&
    // params.at("strict").as_bool();

    // Get info on account.
    std::optional<ripple::AccountID> accountID =
        accountFromStringStrict(strIdent);

    if (!accountID)
    {
        accountID = ripple::AccountID();
        if (!accountID->parseHex(request.at("account").as_string().c_str()))
        {
            response["error"] = "account malformed";
            return response;
        }
    }
    auto key = ripple::keylet::account(accountID.value());

    auto start = std::chrono::system_clock::now();
    std::optional<std::vector<unsigned char>> dbResponse =
        backend.fetchLedgerObject(key.key, *ledgerSequence);
    auto end = std::chrono::system_clock::now();
    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    if (!dbResponse)
    {
        response["error"] = "no response from db";
    }
    ripple::STLedgerEntry sle{
        ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key.key};
    if (!key.check(sle))
    {
        response["error"] = "error fetching record from db";
        return response;
    }
    else
    {
        response["success"] = "fetched successfully!";
        if (!binary)
<<<<<<< HEAD
            response["object"] = toJson(sle);
=======
            response["result"] = getJson(sle);
>>>>>>> 54005ca (save progress)
        else
            response["result"] = ripple::strHex(*dbResponse);
        response["db_time"] = time;
        return response;
    }

    // Return SignerList(s) if that is requested.
    /*
    if (params.isMember(jss::signer_lists) &&
        params[jss::signer_lists].asBool())
    {
        // We put the SignerList in an array because of an anticipated
        // future when we support multiple signer lists on one account.
        Json::Value jvSignerList = Json::arrayValue;

        // This code will need to be revisited if in the future we
        // support multiple SignerLists on one account.
        auto const sleSigners = ledger->read(keylet::signers(accountID));
        if (sleSigners)
            jvSignerList.append(sleSigners->toJson(JsonOptions::none));

        result[jss::account_data][jss::signer_lists] =
            std::move(jvSignerList);
    }
    */

    return response;
}

