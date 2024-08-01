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

#include "rpc/handlers/AccountObjects.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/LedgerUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

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

AccountObjectsHandler::Result
AccountObjectsHandler::process(AccountObjectsHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
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
        typeFilter.emplace();
        auto const& deletionBlockers = util::LedgerTypes::GetDeletionBlockerLedgerTypes();
        typeFilter->reserve(deletionBlockers.size());

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

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(type)))
        input.type = util::LedgerTypes::GetLedgerEntryTypeFromStr(boost::json::value_to<std::string>(jv.at(JS(type))));

    if (jsonObject.contains(JS(limit)))
        input.limit = jv.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(deletion_blockers_only)))
        input.deletionBlockersOnly = jsonObject.at(JS(deletion_blockers_only)).as_bool();

    return input;
}

}  // namespace rpc
