#include "rpc/handlers/Feature.hpp"

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

FeatureHandler::Result
FeatureHandler::process(FeatureHandler::Input const& input, Context const& ctx) const
{
    namespace vs = std::views;
    namespace rg = std::ranges;

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "Feature's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const& all = amendmentCenter_->getAll();

    auto searchPredicate = [search = input.feature](auto const& feature) {
        if (search) {
            return xrpl::to_string(feature.feature) == *search or feature.name == *search;
        }
        return true;
    };

    std::vector<Output::Feature> filtered;
    rg::transform(
        all | vs::filter(searchPredicate), std::back_inserter(filtered), [&](auto const& feature) {
            return Output::Feature{
                .name = feature.name,
                .key = xrpl::to_string(feature.feature),
                .supported = feature.isSupportedByClio and feature.isSupportedByXRPL,
            };
        }
    );

    if (filtered.empty())
        return Error{Status{RippledError::RpcBadFeature}};

    std::vector<data::AmendmentKey> names;
    rg::transform(filtered, std::back_inserter(names), [](auto const& feature) {
        return feature.name;
    });

    std::map<std::string, Output::Feature> features;
    rg::transform(
        filtered,
        amendmentCenter_->isEnabled(ctx.yield, names, lgrInfo.seq),
        std::inserter(features, std::end(features)),
        [&](Output::Feature feature, bool isEnabled) {
            feature.enabled = isEnabled;
            return std::make_pair(feature.key, std::move(feature));
        }
    );

    return Output{
        .features = std::move(features),
        .ledgerHash = xrpl::strHex(lgrInfo.hash),
        .ledgerIndex = lgrInfo.seq,
        .inlineResult = input.feature.has_value()
    };
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    FeatureHandler::Output const& output
)
{
    using boost::json::value_from;

    if (output.inlineResult) {
        jv = value_from(output.features);
    } else {
        jv = {
            {JS(features), value_from(output.features)},
        };
    }

    auto& obj = jv.as_object();
    obj[JS(ledger_hash)] = output.ledgerHash;
    obj[JS(ledger_index)] = output.ledgerIndex;
    obj[JS(validated)] = output.validated;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    FeatureHandler::Output::Feature const& feature
)
{
    using boost::json::value_from;

    jv = {
        {JS(name), feature.name},
        {JS(enabled), feature.enabled},
        {JS(supported), feature.supported},
    };
}

}  // namespace rpc
