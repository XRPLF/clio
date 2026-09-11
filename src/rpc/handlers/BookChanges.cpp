#include "rpc/handlers/BookChanges.hpp"

#include "data/Types.hpp"
#include "rpc/BookChangesHelper.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <vector>

namespace rpc {

BookChangesHandler::Result
BookChangesHandler::process(BookChangesHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "BookChanges' ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const transactions =
        sharedPtrBackend_->fetchAllTransactionsInLedger(lgrInfo.seq, ctx.yield);

    Output response;
    response.bookChanges = BookChanges::compute(transactions);
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.ledgerTime = lgrInfo.closeTime.time_since_epoch().count();

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    BookChangesHandler::Output const& output
)
{
    using boost::json::value_from;

    jv = {
        {JS(type), "bookChanges"},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(ledger_time), output.ledgerTime},
        {JS(validated), output.validated},
        {JS(changes), value_from(output.bookChanges)},
    };
}

[[nodiscard]] boost::json::object
computeBookChanges(
    xrpl::LedgerHeader const& lgrInfo,
    std::vector<data::TransactionAndMetadata> const& transactions
)
{
    using boost::json::value_from;

    return {
        {JS(type), "bookChanges"},
        {JS(ledger_index), lgrInfo.seq},
        {JS(ledger_hash), to_string(lgrInfo.hash)},
        {JS(ledger_time), lgrInfo.closeTime.time_since_epoch().count()},
        {JS(changes), value_from(BookChanges::compute(transactions))},
    };
}

}  // namespace rpc
