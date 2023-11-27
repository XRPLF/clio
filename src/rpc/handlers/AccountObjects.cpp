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

#include "rpc/handlers/AccountObjects.h"
#include <ripple/basics/strHex.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/Types.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

// found here : https://xrpl.org/ledger_entry.html#:~:text=valid%20fields%20are%3A-,index,-account_root
std::unordered_map<std::string, ripple::LedgerEntryType> const AccountObjectsHandler::TYPES_MAP{
    {JS(amm), ripple::ltAMM},
    {JS(state), ripple::ltRIPPLE_STATE},
    {JS(ticket), ripple::ltTICKET},
    {JS(signer_list), ripple::ltSIGNER_LIST},
    {JS(payment_channel), ripple::ltPAYCHAN},
    {JS(offer), ripple::ltOFFER},
    {JS(escrow), ripple::ltESCROW},
    {JS(deposit_preauth), ripple::ltDEPOSIT_PREAUTH},
    {JS(check), ripple::ltCHECK},
    {JS(nft_page), ripple::ltNFTOKEN_PAGE},
    {JS(nft_offer), ripple::ltNFTOKEN_OFFER},
    {JS(did), ripple::ltDID},
};

std::unordered_set<std::string> const AccountObjectsHandler::TYPES_KEYS = [] {
    std::unordered_set<std::string> keys;
    std::transform(TYPES_MAP.begin(), TYPES_MAP.end(), std::inserter(keys, keys.begin()), [](auto const& pair) {
        return pair.first;
    });
    return keys;
}();

AccountObjectsHandler::Result
AccountObjectsHandler::process(AccountObjectsHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto typeFilter = std::optional<std::vector<ripple::LedgerEntryType>>{};

    if (input.deletionBlockersOnly) {
        static constexpr ripple::LedgerEntryType deletionBlockers[] = {
            ripple::ltCHECK,
            ripple::ltESCROW,
            ripple::ltNFTOKEN_PAGE,
            ripple::ltPAYCHAN,
            ripple::ltRIPPLE_STATE,
        };

        typeFilter.emplace();
        typeFilter->reserve(std::size(deletionBlockers));

        for (auto type : deletionBlockers) {
            if (input.type && input.type != type)
                continue;

            typeFilter->push_back(type);
        }
    } else {
        if (input.type && input.type != ripple::ltANY)
            typeFilter = {*input.type};
    }

    Output response;
    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (not typeFilter or
            std::find(std::begin(typeFilter.value()), std::end(typeFilter.value()), sle.getType()) !=
                std::end(typeFilter.value())) {
            response.accountObjects.push_back(std::move(sle));
        }
        return true;
    };

    auto const next = traverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse, true
    );

    if (auto status = std::get_if<Status>(&next))
        return Error{*status};

    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.limit = input.limit;
    response.account = input.account;

    auto const& nextMarker = std::get<AccountCursor>(next);

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountObjectsHandler::Output const& output)
{
    auto objects = boost::json::array{};
    std::transform(
        std::cbegin(output.accountObjects),
        std::cend(output.accountObjects),
        std::back_inserter(objects),
        [](auto const& sle) { return toJson(sle); }
    );

    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(account), output.account},
        {JS(account_objects), objects},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = *(output.marker);
}

AccountObjectsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountObjectsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountObjectsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = jv.at(JS(account)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(type)))
        input.type = AccountObjectsHandler::TYPES_MAP.at(jv.at(JS(type)).as_string().c_str());

    if (jsonObject.contains(JS(limit)))
        input.limit = jv.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jv.at(JS(marker)).as_string().c_str();

    if (jsonObject.contains(JS(deletion_blockers_only)))
        input.deletionBlockersOnly = jsonObject.at(JS(deletion_blockers_only)).as_bool();

    return input;
}

}  // namespace rpc
