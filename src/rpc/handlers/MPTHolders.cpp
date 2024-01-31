//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/handlers/MPTHolders.h"

#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/Types.h"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/nft.h>

#include <optional>
#include <string>
#include <variant>

using namespace ripple;

namespace rpc {

MPTHoldersHandler::Result
MPTHoldersHandler::process(MPTHoldersHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );
    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerInfo>(lgrInfoOrStatus);

    auto const limit = input.limit.value_or(MPTHoldersHandler::LIMIT_DEFAULT);

    auto const mptID = ripple::uint192{input.mptID.c_str()};

    auto const issuanceLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::mptIssuance(mptID).key, lgrInfo.seq, ctx.yield);

    if (!issuanceLedgerObject)
        return Error{Status{RippledError::rpcOBJECT_NOT_FOUND, "objectNotFound"}};

    std::optional<ripple::AccountID> cursor;
    if (input.marker)
        cursor = ripple::AccountID{input.marker->c_str()};

    auto const dbResponse =
        sharedPtrBackend_->fetchMPTHolders(mptID, limit, cursor, lgrInfo.seq, ctx.yield);

    auto output = MPTHoldersHandler::Output{};

    output.mptID = to_string(mptID);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;

    boost::json::array mpts;
    for (auto const& mpt : dbResponse.mptokens) {
        ripple::STLedgerEntry sle{ripple::SerialIter{mpt.data(), mpt.size()}, keylet::mptIssuance(mptID).key};
        boost::json::object mptJson;

        mptJson[JS(account)] = toBase58(sle.getAccountID(ripple::sfAccount));
        mptJson[JS(flags)] = sle[ripple::sfFlags];
        mptJson["mpt_amount"] = toBoostJson(ripple::STUInt64{sle[ripple::sfMPTAmount]}.getJson(JsonOptions::none));

        if (!sle[~ripple::sfLockedAmount])
            mptJson["locked_amount"] = toBoostJson(ripple::STUInt64{sle[~ripple::sfLockedAmount].value_or(0)}.getJson(JsonOptions::none));
        
        mptJson["mptoken_index"] = ripple::to_string(ripple::keylet::mptoken(mptID, sle.getAccountID(ripple::sfAccount)).key);  
        output.mpts.push_back(mptJson);
    }

    if (dbResponse.cursor.has_value())
        output.marker = strHex(*dbResponse.cursor);

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MPTHoldersHandler::Output const& output)
{
    jv = {
        {JS(mpt_issuance_id), output.mptID},
        {JS(limit), output.limit},
        {JS(ledger_index), output.ledgerIndex},
        {"mptokens", output.mpts},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = *(output.marker);
}

MPTHoldersHandler::Input
tag_invoke(boost::json::value_to_tag<MPTHoldersHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    MPTHoldersHandler::Input input;

    input.mptID = jsonObject.at(JS(mpt_issuance_id)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();

    return input;
}
}  // namespace rpc
