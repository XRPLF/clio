//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "rpc/handlers/DepositAuthorized.hpp"

#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace rpc {

DepositAuthorizedHandler::Result
DepositAuthorizedHandler::process(DepositAuthorizedHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const sourceAccountID = accountFromStringStrict(input.sourceAccount);
    auto const destinationAccountID = accountFromStringStrict(input.destinationAccount);

    auto const srcAccountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*sourceAccountID).key, lgrInfo.seq, ctx.yield);

    if (!srcAccountLedgerObject)
        return Error{Status{RippledError::rpcSRC_ACT_NOT_FOUND, "source_accountNotFound"}};

    auto const dstKeylet = ripple::keylet::account(*destinationAccountID).key;
    auto const dstAccountLedgerObject = sharedPtrBackend_->fetchLedgerObject(dstKeylet, lgrInfo.seq, ctx.yield);

    if (!dstAccountLedgerObject)
        return Error{Status{RippledError::rpcDST_ACT_NOT_FOUND, "destination_accountNotFound"}};

    Output response;

    auto it = ripple::SerialIter{dstAccountLedgerObject->data(), dstAccountLedgerObject->size()};
    auto const sleDest = ripple::SLE{it, dstKeylet};
    bool const reqAuth = sleDest.isFlag(ripple::lsfDepositAuth) && (sourceAccountID != destinationAccountID);
    auto const& creds = input.credentials;
    bool const credentialsPresent = creds.has_value();

    ripple::STArray authCreds;
    if (credentialsPresent) {
        if (creds.value().empty()) {
            return Error{Status{RippledError::rpcINVALID_PARAMS, "credential array has no elements."}};
        }
        if (creds.value().size() > ripple::maxCredentialsArraySize) {
            return Error{Status{RippledError::rpcINVALID_PARAMS, "credential array too long."}};
        }
        auto const credArray = credentials::fetchCredentialArray(
            input.credentials, *sourceAccountID, *sharedPtrBackend_, lgrInfo, ctx.yield
        );
        if (!credArray.has_value())
            return Error{std::move(credArray).error()};
        authCreds = std::move(credArray).value();
    }

    // If the two accounts are the same OR if that flag is
    // not set, then the deposit should be fine.
    bool depositAuthorized = true;

    if (reqAuth) {
        ripple::uint256 hashKey;
        if (credentialsPresent) {
            auto const sortedAuthCreds = credentials::createAuthCredentials(authCreds);
            ASSERT(
                sortedAuthCreds.size() == authCreds.size(), "should already be checked above that there is no duplicate"
            );

            hashKey = ripple::keylet::depositPreauth(*destinationAccountID, sortedAuthCreds).key;
        } else {
            hashKey = ripple::keylet::depositPreauth(*destinationAccountID, *sourceAccountID).key;
        }

        depositAuthorized = sharedPtrBackend_->fetchLedgerObject(hashKey, lgrInfo.seq, ctx.yield).has_value();
    }

    response.sourceAccount = input.sourceAccount;
    response.destinationAccount = input.destinationAccount;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.depositAuthorized = depositAuthorized;
    if (credentialsPresent)
        response.credentials = input.credentials.value();

    return response;
}

DepositAuthorizedHandler::Input
tag_invoke(boost::json::value_to_tag<DepositAuthorizedHandler::Input>, boost::json::value const& jv)
{
    auto input = DepositAuthorizedHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.sourceAccount = boost::json::value_to<std::string>(jv.at(JS(source_account)));
    input.destinationAccount = boost::json::value_to<std::string>(jv.at(JS(destination_account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(credentials))) {
        input.credentials = boost::json::value_to<boost::json::array>(jv.at(JS(credentials)));
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, DepositAuthorizedHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(deposit_authorized), output.depositAuthorized},
        {JS(source_account), output.sourceAccount},
        {JS(destination_account), output.destinationAccount},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated}
    };
    if (output.credentials)
        jv.as_object()[JS(credentials)] = *output.credentials;
}

}  // namespace rpc
