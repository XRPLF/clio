#include "rpc/handlers/BookOffers.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <string>

namespace rpc {

BookOffersHandler::Result
BookOffersHandler::process(Input const& input, Context const& ctx) const
{
    auto bookMaybe = parseBook(input.takerPays, input.takerGets, input.domain);
    if (!bookMaybe.has_value())
        return Error{bookMaybe.error()};

    // check ledger
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "BookOffer's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const book = *bookMaybe;
    auto const bookKey = getBookBase(book);

    // TODO: Add performance metrics if needed in future
    auto [offers, _] =
        sharedPtrBackend_->fetchBookOffers(bookKey, lgrInfo.seq, input.limit, ctx.yield);

    auto output = BookOffersHandler::Output{};
    output.ledgerHash = xrpl::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;
    output.offers = postProcessOrderBook(
        offers,
        book,
        input.taker ? *(input.taker) : beast::kZero,
        *sharedPtrBackend_,
        *amendmentCenter_,
        lgrInfo.seq,
        ctx.yield
    );

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    BookOffersHandler::Output const& output
)
{
    jv = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(offers), output.offers},
    };
}

}  // namespace rpc
