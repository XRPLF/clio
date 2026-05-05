#include "rpc/handlers/BookChanges.hpp"

#include "data/Types.hpp"
#include "rpc/BookChangesHelper.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
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
    auto const transactions =
        sharedPtrBackend_->fetchAllTransactionsInLedger(lgrInfo.seq, ctx.yield);

    Output response;
    response.bookChanges = BookChanges::compute(transactions);
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
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

BookChangesHandler::Input
tag_invoke(boost::json::value_to_tag<BookChangesHandler::Input>, boost::json::value const& jv)
{
    auto input = BookChangesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

[[nodiscard]] boost::json::object
computeBookChanges(
    ripple::LedgerHeader const& lgrInfo,
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
