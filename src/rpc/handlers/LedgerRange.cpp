#include "rpc/handlers/LedgerRange.hpp"

#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/jss.h>

#include <optional>

namespace rpc {

LedgerRangeHandler::Result
LedgerRangeHandler::process([[maybe_unused]] Context const& ctx) const
{
    // note: we can't get here if range is not available so it's safe
    return Output{sharedPtrBackend_->fetchLedgerRange().value()};
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    LedgerRangeHandler::Output const& output
)
{
    jv = boost::json::object{
        {JS(ledger_index_min), output.range.minSequence},
        {JS(ledger_index_max), output.range.maxSequence},
    };
}

}  // namespace rpc
