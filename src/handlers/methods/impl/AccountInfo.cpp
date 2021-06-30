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
#include <handlers/methods/Account.h>
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

namespace RPC
{

Result
doAccountInfo(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    std::string strIdent;
    if (request.contains("account"))
        strIdent = request.at("account").as_string().c_str();
    else if (request.contains("ident"))
        strIdent = request.at("ident").as_string().c_str();
    else
        return Status{Error::rpcACT_MALFORMED};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    // Get info on account.
    auto accountID = accountFromStringStrict(strIdent);

    auto key = ripple::keylet::account(accountID.value());

    auto start = std::chrono::system_clock::now();
    std::optional<std::vector<unsigned char>> dbResponse =
        context.backend->fetchLedgerObject(key.key, lgrInfo.seq);
    auto end = std::chrono::system_clock::now();

    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    if (!dbResponse)
    {
        return Status{Error::rpcACT_NOT_FOUND};
    }

    ripple::STLedgerEntry sle{
        ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key.key};

    if (!key.check(sle))
        return Status{Error::rpcDB_DESERIALIZATION};

    // if (!binary)
    //     response["account_data"] = getJson(sle);
    // else
    //     response["account_data"] = ripple::strHex(*dbResponse);
    // response["db_time"] = time;

    response["account_data"] = toJson(sle);
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

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
            jvSignerList.append(sleSigners->getJson(JsonOptions::none));

        result[jss::account_data][jss::signer_lists] =
            std::move(jvSignerList);
    }
    */

    return response;
}

} //namespace RPC