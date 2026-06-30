#include "rpc/handlers/BookOffers.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <string>

namespace rpc {

BookOffersHandler::Result
BookOffersHandler::process(Input const& input, Context const& ctx) const
{
    auto bookMaybe = parseBook(input.paysAsset, input.getsAsset, input.domain);
    if (!bookMaybe.has_value())
        return Error{bookMaybe.error()};

    // check ledger
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "BookOffer's ledger range must be available");

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

BookOffersHandler::Input
tag_invoke(boost::json::value_to_tag<BookOffersHandler::Input>, boost::json::value const& jv)
{
    auto input = BookOffersHandler::Input{};
    auto const& jsonObject = jv.as_object();

    auto const parseAsset = [](boost::json::object const& obj) -> xrpl::Asset {
        if (obj.contains(JS(mpt_issuance_id))) {
            xrpl::MPTID mptid;
            auto const parsed =
                mptid.parseHex(boost::json::value_to<std::string>(obj.at(JS(mpt_issuance_id))));
            ASSERT(parsed, "mpt_issuance_id is guaranteed to be valid hex by the spec validator");
            return xrpl::MPTIssue{mptid};
        }
        xrpl::Issue issue;
        auto const currencyParsed = xrpl::toCurrency(
            issue.currency, boost::json::value_to<std::string>(obj.at(JS(currency)))
        );
        ASSERT(currencyParsed, "currency is guaranteed to be valid by the spec validator");
        if (obj.contains(JS(issuer))) {
            auto const issuerParsed = xrpl::toIssuer(
                issue.account, boost::json::value_to<std::string>(obj.at(JS(issuer)))
            );
            ASSERT(issuerParsed, "issuer is guaranteed to be valid by the spec validator");
        }
        return issue;
    };

    input.getsAsset = parseAsset(jv.at(JS(taker_gets)).as_object());
    input.paysAsset = parseAsset(jv.at(JS(taker_pays)).as_object());

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(taker)))
        input.taker = accountFromStringStrict(boost::json::value_to<std::string>(jv.at(JS(taker))));

    if (jsonObject.contains(JS(domain)))
        input.domain = boost::json::value_to<std::string>(jv.at(JS(domain)));

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    return input;
}

}  // namespace rpc
