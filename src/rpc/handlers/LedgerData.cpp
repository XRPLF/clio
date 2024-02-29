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

#include "rpc/handlers/LedgerData.hpp"

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/LedgerUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/serialize.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

LedgerDataHandler::Result
LedgerDataHandler::process(Input input, Context const& ctx) const
{
    // marker must be int if outOfOrder is true
    if (input.outOfOrder && input.marker)
        return Error{Status{RippledError::rpcINVALID_PARAMS, "outOfOrderMarkerNotInt"}};

    if (!input.outOfOrder && input.diffMarker)
        return Error{Status{RippledError::rpcINVALID_PARAMS, "markerNotString"}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);

    Output output;

    // no marker -> first call, return header information
    if ((!input.marker) && (!input.diffMarker)) {
        output.header = toJson(lgrInfo, input.binary, ctx.apiVersion);
    } else {
        if (input.marker && !sharedPtrBackend_->fetchLedgerObject(*(input.marker), lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::rpcINVALID_PARAMS, "markerDoesNotExist"}};
    }

    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;

    auto const start = std::chrono::system_clock::now();
    std::vector<data::LedgerObject> results;

    if (input.diffMarker) {
        // keep the same logic as previous implementation
        auto diff = sharedPtrBackend_->fetchLedgerDiff(*(input.diffMarker), ctx.yield);
        std::vector<ripple::uint256> keys;

        for (auto& [key, object] : diff) {
            if (object.empty())
                keys.push_back(key);
        }

        auto objs = sharedPtrBackend_->fetchLedgerObjects(keys, lgrInfo.seq, ctx.yield);

        for (size_t i = 0; i < objs.size(); ++i) {
            auto& obj = objs[i];
            if (!obj.empty())
                results.push_back({keys[i], std::move(obj)});
        }

        if (*(input.diffMarker) > lgrInfo.seq)
            output.diffMarker = *(input.diffMarker) - 1;
    } else {
        // limit's limitation is different based on binary or json
        // framework can not handler the check right now, adjust the value here
        auto const limit =
            std::min(input.limit, input.binary ? LedgerDataHandler::LIMITBINARY : LedgerDataHandler::LIMITJSON);
        auto page = sharedPtrBackend_->fetchLedgerPage(input.marker, lgrInfo.seq, limit, input.outOfOrder, ctx.yield);
        results = std::move(page.objects);

        if (page.cursor) {
            output.marker = ripple::strHex(*(page.cursor));
        } else if (input.outOfOrder) {
            output.diffMarker = sharedPtrBackend_->fetchLedgerRange()->maxSequence;
        }
    }

    auto const end = std::chrono::system_clock::now();
    LOG(log_.debug()) << "Number of results = " << results.size() << " fetched in "
                      << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds";

    output.states.reserve(results.size());

    for (auto const& [key, object] : results) {
        ripple::STLedgerEntry const sle{ripple::SerialIter{object.data(), object.size()}, key};

        // note the filter is after limit is applied, same as rippled
        if (input.type == ripple::LedgerEntryType::ltANY || sle.getType() == input.type) {
            if (input.binary) {
                boost::json::object entry;
                entry[JS(data)] = ripple::serializeHex(sle);
                entry[JS(index)] = ripple::to_string(sle.key());
                output.states.push_back(std::move(entry));
            } else {
                auto sleJson = toJson(sle);

                // if object type if mpt issuance, inject synthetic mpt id
                if (sle.getType() == ripple::ltMPTOKEN_ISSUANCE)
                    sleJson[JS(mpt_issuance_id)] = ripple::to_string(
                        ripple::getMptID(sle.getAccountID(ripple::sfIssuer), sle.getFieldU32(ripple::sfSequence))
                    );

                output.states.push_back(sleJson);
            }
        }
    }

    if (input.outOfOrder)
        output.cacheFull = sharedPtrBackend_->cache().isFull();

    auto const end2 = std::chrono::system_clock::now();
    LOG(log_.debug()) << "Number of results = " << results.size() << " serialized in "
                      << std::chrono::duration_cast<std::chrono::microseconds>(end2 - end).count() << " microseconds";

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerDataHandler::Output const& output)
{
    auto obj = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(state), output.states},
    };

    if (output.header)
        obj[JS(ledger)] = *(output.header);

    if (output.cacheFull)
        obj["cache_full"] = *(output.cacheFull);

    if (output.diffMarker) {
        obj[JS(marker)] = *(output.diffMarker);
    } else if (output.marker) {
        obj[JS(marker)] = *(output.marker);
    }

    jv = std::move(obj);
}

LedgerDataHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerDataHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerDataHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(binary))) {
        input.binary = jsonObject.at(JS(binary)).as_bool();
        input.limit = input.binary ? LedgerDataHandler::LIMITBINARY : LedgerDataHandler::LIMITJSON;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains("out_of_order"))
        input.outOfOrder = jsonObject.at("out_of_order").as_bool();

    if (jsonObject.contains("marker")) {
        if (jsonObject.at("marker").is_string()) {
            input.marker = ripple::uint256{boost::json::value_to<std::string>(jsonObject.at("marker")).data()};
        } else {
            input.diffMarker = jsonObject.at("marker").as_int64();
        }
    }

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jsonObject.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(type)))
        input.type = util::getLedgerEntryTypeFromStr(boost::json::value_to<std::string>(jsonObject.at(JS(type))));

    return input;
}

}  // namespace rpc
