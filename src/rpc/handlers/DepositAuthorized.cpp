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

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/jss.h>

#include <string>

namespace rpc {

static DepositAuthorizedHandler::Result
DepositAuthorizedHandler::process(DepositAuthorizedHandler::Input input, Context const& ctx)
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
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

    response.sourceAccount = input.sourceAccount;
    response.destinationAccount = input.destinationAccount;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    // If the two accounts are the same, then the deposit should be fine.
    if (sourceAccountID != destinationAccountID) {
        auto it = ripple::SerialIter{dstAccountLedgerObject->data(), dstAccountLedgerObject->size()};
        auto sle = ripple::SLE{it, dstKeylet};

        // Check destination for the DepositAuth flag.
        // If that flag is not set then a deposit should be just fine.
        if ((sle.getFieldU32(ripple::sfFlags) & ripple::lsfDepositAuth) != 0u) {
            // See if a preauthorization entry is in the ledger.
            auto const depositPreauthKeylet = ripple::keylet::depositPreauth(*destinationAccountID, *sourceAccountID);
            auto const sleDepositAuth =
                sharedPtrBackend_->fetchLedgerObject(depositPreauthKeylet.key, lgrInfo.seq, ctx.yield);
            response.depositAuthorized = static_cast<bool>(sleDepositAuth);
        }
    }

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
        {JS(validated), output.validated},
    };
}

}  // namespace rpc
