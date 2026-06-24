#include "rpc/handlers/LedgerData.hpp"

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
#include "util/LedgerUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/serialize.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

LedgerDataHandler::Result
LedgerDataHandler::process(Input const& input, Context const& ctx) const
{
    // marker must be int if outOfOrder is true
    if (input.outOfOrder && input.marker)
        return Error{Status{RippledError::RpcInvalidParams, "outOfOrderMarkerNotInt"}};

    if (!input.outOfOrder && input.diffMarker)
        return Error{Status{RippledError::RpcInvalidParams, "markerNotString"}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerData's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    Output output;

    // no marker -> first call, return header information
    if ((!input.marker) && (!input.diffMarker)) {
        output.header = toJson(lgrInfo, input.binary, ctx.apiVersion);
    } else {
        if (input.marker &&
            !sharedPtrBackend_->fetchLedgerObject(*(input.marker), lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::RpcInvalidParams, "markerDoesNotExist"}};
    }

    output.ledgerHash = xrpl::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;

    auto const start = std::chrono::system_clock::now();
    std::vector<data::LedgerObject> results;

    if (input.diffMarker) {
        // keep the same logic as previous implementation
        auto diff = sharedPtrBackend_->fetchLedgerDiff(*(input.diffMarker), ctx.yield);
        std::vector<xrpl::uint256> keys;

        for (auto& [key, object] : diff) {
            if (object.empty())
                keys.push_back(key);
        }

        auto objs = sharedPtrBackend_->fetchLedgerObjects(keys, lgrInfo.seq, ctx.yield);

        for (size_t i = 0; i < objs.size(); ++i) {
            auto& obj = objs[i];
            if (!obj.empty())
                results.push_back({.key = keys[i], .blob = std::move(obj)});
        }

        if (*(input.diffMarker) > lgrInfo.seq)
            output.diffMarker = *(input.diffMarker) - 1;
    } else {
        // limit's limitation is different based on binary or json
        // framework can not handler the check right now, adjust the value here
        auto const limit = std::min(
            input.limit,
            input.binary ? LedgerDataHandler::kLimitBinary : LedgerDataHandler::kLimitJson
        );
        auto page = sharedPtrBackend_->fetchLedgerPage(
            input.marker, lgrInfo.seq, limit, input.outOfOrder, ctx.yield
        );
        results = std::move(page.objects);

        if (page.cursor) {
            output.marker = xrpl::strHex(*(page.cursor));
        } else if (input.outOfOrder) {
            output.diffMarker = range->maxSequence;  // NOLINT(bugprone-unchecked-optional-access)
        }
    }

    auto const end = std::chrono::system_clock::now();
    LOG(log_.debug()) << "Number of results = " << results.size() << " fetched in "
                      << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
                      << " microseconds";

    output.states.reserve(results.size());

    for (auto const& [key, object] : results) {
        xrpl::STLedgerEntry const sle{xrpl::SerialIter{object.data(), object.size()}, key};

        // note the filter is after limit is applied, same as rippled
        if (input.type == xrpl::LedgerEntryType::ltANY || sle.getType() == input.type) {
            if (input.binary) {
                boost::json::object entry;
                entry[JS(data)] = xrpl::serializeHex(sle);
                entry[JS(index)] = xrpl::to_string(sle.key());
                output.states.push_back(std::move(entry));
            } else {
                output.states.push_back(toJson(sle));
            }
        }
    }

    if (input.outOfOrder)
        output.cacheFull = sharedPtrBackend_->cache().isFull();

    auto const end2 = std::chrono::system_clock::now();
    LOG(log_.debug()) << "Number of results = " << results.size() << " serialized in "
                      << std::chrono::duration_cast<std::chrono::microseconds>(end2 - end).count()
                      << " microseconds";

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    LedgerDataHandler::Output const& output
)
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
        input.limit =
            input.binary ? LedgerDataHandler::kLimitBinary : LedgerDataHandler::kLimitJson;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jsonObject.at(JS(limit)));

    if (jsonObject.contains("out_of_order"))
        input.outOfOrder = jsonObject.at("out_of_order").as_bool();

    if (jsonObject.contains(JS(marker))) {
        if (jsonObject.at(JS(marker)).is_string()) {
            input.marker =
                xrpl::uint256{boost::json::value_to<std::string>(jsonObject.at(JS(marker))).data()};
        } else {
            input.diffMarker = util::integralValueAs<uint32_t>(jsonObject.at(JS(marker)));
        }
    }

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(type))) {
        input.type = util::LedgerTypes::getLedgerEntryTypeFromStr(
            boost::json::value_to<std::string>(jv.at(JS(type)))
        );
    }

    return input;
}

}  // namespace rpc
