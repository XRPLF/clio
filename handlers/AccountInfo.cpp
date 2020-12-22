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
#include <reporting/ReportingBackend.h>

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    boost::optional<ripple::AccountID> result;

    auto const publicKey = ripple::parseBase58<ripple::PublicKey>(
        ripple::TokenType::AccountPublic, account);

    if (publicKey)
        result = ripple::calcAccountID(*publicKey);
    else
        result = ripple::parseBase58<ripple::AccountID>(account);

    if (result)
        return result.value();
    else
        return {};
}
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
    CassandraFlatMapBackend const& backend)
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
    size_t ledgerSequence = request.at("ledger_index").as_int64();

    // bool bStrict = request.contains("strict") &&
    // params.at("strict").as_bool();

    // Get info on account.
    std::optional<ripple::AccountID> accountID =
        accountFromStringStrict(strIdent);

    if (!accountID)
    {
        response["error"] = "couldnt decode account";
        return response;
    }
    auto key = ripple::keylet::account(accountID.value());

    std::optional<std::vector<unsigned char>> dbResponse =
        backend.fetch(key.key.data(), ledgerSequence);
    if (!dbResponse)
    {
        response["error"] = "no response from db";
    }
    auto sle = std::make_shared<ripple::SLE>(
        ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key.key);
    if (!key.check(*sle))
    {
        response["error"] = "error fetching record from db";
        return response;
    }
    else
    {
        response["success"] = "fetched successfully!";
        response["object"] = sle->getFullText();
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
            jvSignerList.append(sleSigners->getJson(JsonOptions::none));

        result[jss::account_data][jss::signer_lists] =
            std::move(jvSignerList);
    }
    */

    return response;
}
