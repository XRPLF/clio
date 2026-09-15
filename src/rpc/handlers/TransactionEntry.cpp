#include "rpc/handlers/TransactionEntry.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <utility>

namespace rpc {

TransactionEntryHandler::Result
TransactionEntryHandler::process(
    TransactionEntryHandler::Input const& input,
    Context const& ctx
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "TransactionEntry's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto output = TransactionEntryHandler::Output{};
    output.apiVersion = ctx.apiVersion;

    output.ledgerHeader = *expectedLgrInfo;
    auto const dbRet = sharedPtrBackend_->fetchTransaction(input.txHash, ctx.yield);
    // Note: transaction_entry is meant to only search a specified ledger for
    // the specified transaction. tx searches the entire range of history. For
    // rippled, having two separate commands made sense, as tx would use SQLite
    // and transaction_entry used the nodestore. For clio though, there is no
    // difference between the implementation of these two, as clio only stores
    // transactions in a transactions table, where the key is the hash. However,
    // the API for transaction_entry says the method only searches the specified
    // ledger; we simulate that here by returning not found if the transaction
    // is in a different ledger than the one specified.
    if (!dbRet || dbRet->ledgerSequence != output.ledgerHeader->seq) {
        return Error{
            Status{RippledError::RpcTxnNotFound, "transactionNotFound", "Transaction not found."}
        };
    }

    auto [txn, meta] = toExpandedJson(*dbRet, ctx.apiVersion);

    output.tx = std::move(txn);
    output.metadata = std::move(meta);

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    TransactionEntryHandler::Output const& output
)
{
    auto const metaKey = output.apiVersion > 1u ? JS(meta) : JS(metadata);

    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    jv = {
        {JS(validated), output.validated},
        {metaKey, output.metadata},
        {JS(tx_json), output.tx},
        {JS(ledger_index), output.ledgerHeader->seq},
        {JS(ledger_hash), xrpl::strHex(output.ledgerHeader->hash)},
    };
    // NOLINTEND(bugprone-unchecked-optional-access)

    if (output.apiVersion > 1u) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        jv.as_object()[JS(close_time_iso)] = xrpl::toStringIso(output.ledgerHeader->closeTime);
        if (output.tx.contains(JS(hash))) {
            jv.as_object()[JS(hash)] = output.tx.at(JS(hash));
            jv.as_object()[JS(tx_json)].as_object().erase(JS(hash));
        }
    }
}

}  // namespace rpc
