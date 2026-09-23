#include "rpc/handlers/LedgerData.hpp"

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>
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
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

LedgerDataHandler::Result
LedgerDataHandler::process(Input const& input, Context const& ctx) const
{
    using rpc::spec::handlers::ledger_data::MarkerValue;

    auto const* uint256Marker = input.marker ? std::get_if<xrpl::uint256>(&*input.marker) : nullptr;
    auto const* diffMarker = input.marker ? std::get_if<uint32_t>(&*input.marker) : nullptr;

    if (input.outOfOrder && uint256Marker != nullptr)
        return Error{Status{RippledError::RpcInvalidParams, "outOfOrderMarkerNotInt"}};

    if (!input.outOfOrder && diffMarker != nullptr)
        return Error{Status{RippledError::RpcInvalidParams, "markerNotString"}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerData's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    Output output;

    // no marker -> first call, return header information
    if (not input.marker.has_value()) {
        output.header = toJson(lgrInfo, input.binary, ctx.apiVersion);
    } else {
        if (uint256Marker != nullptr &&
            !sharedPtrBackend_->fetchLedgerObject(*uint256Marker, lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::RpcInvalidParams, "markerDoesNotExist"}};
    }

    output.ledgerHash = xrpl::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;

    auto const start = std::chrono::system_clock::now();
    std::vector<data::LedgerObject> results;

    if (diffMarker != nullptr) {
        // keep the same logic as previous implementation
        auto diff = sharedPtrBackend_->fetchLedgerDiff(*diffMarker, ctx.yield);
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

        if (*diffMarker > lgrInfo.seq)
            output.diffMarker = *diffMarker - 1;
    } else {
        auto const maxLimit =
            input.binary ? LedgerDataHandler::kLimitBinary : LedgerDataHandler::kLimitJson;
        auto const limit = std::min(input.limit.value_or(maxLimit), maxLimit);

        std::optional<xrpl::uint256> pageMarker;
        if (uint256Marker != nullptr)
            pageMarker = *uint256Marker;

        auto page = sharedPtrBackend_->fetchLedgerPage(
            pageMarker, lgrInfo.seq, limit, input.outOfOrder, ctx.yield
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

}  // namespace rpc
