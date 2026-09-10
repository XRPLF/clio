#include "rpc/handlers/AccountCurrencies.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <limits>
#include <string>

namespace rpc {
AccountCurrenciesHandler::Result
AccountCurrenciesHandler::process(
    AccountCurrenciesHandler::Input const& input,
    Context const& ctx
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountCurrencies' ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const& accountID = input.account;

    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::account(accountID).key, lgrInfo.seq, ctx.yield
    );
    if (!accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

    Output response;
    auto const addToResponse = [&](xrpl::SLE const sle) {
        if (sle.getType() == xrpl::ltRIPPLE_STATE) {
            auto balance = sle.getFieldAmount(xrpl::sfBalance);
            auto const lowLimit = sle.getFieldAmount(xrpl::sfLowLimit);
            auto const highLimit = sle.getFieldAmount(xrpl::sfHighLimit);
            bool const viewLowest = (lowLimit.getIssuer() == accountID);
            auto const lineLimit = viewLowest ? lowLimit : highLimit;
            auto const lineLimitPeer = !viewLowest ? lowLimit : highLimit;

            if (!viewLowest)
                balance.negate();

            if (balance < lineLimit) {
                response.receiveCurrencies.insert(
                    xrpl::to_string(balance.get<xrpl::Issue>().currency)
                );
            }

            if ((-balance) < lineLimitPeer) {
                response.sendCurrencies.insert(
                    xrpl::to_string(balance.get<xrpl::Issue>().currency)
                );
            }
        }

        return true;
    };

    // traverse all owned nodes, limit->max, marker->empty
    traverseOwnedNodes(
        *sharedPtrBackend_,
        accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        ctx.yield,
        addToResponse
    );

    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountCurrenciesHandler::Output const& output
)
{
    using boost::json::value_from;

    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(receive_currencies), value_from(output.receiveCurrencies)},
        {JS(send_currencies), value_from(output.sendCurrencies)},
    };
}

}  // namespace rpc
