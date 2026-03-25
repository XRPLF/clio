#include "rpc/handlers/TransactionEntry.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
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

    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto output = TransactionEntryHandler::Output{};
    output.apiVersion = ctx.apiVersion;

    output.ledgerHeader = expectedLgrInfo.value();
    auto const dbRet =
        sharedPtrBackend_->fetchTransaction(ripple::uint256{input.txHash.c_str()}, ctx.yield);
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
            Status{RippledError::rpcTXN_NOT_FOUND, "transactionNotFound", "Transaction not found."}
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
    jv = {
        {JS(validated), output.validated},
        {metaKey, output.metadata},
        {JS(tx_json), output.tx},
        {JS(ledger_index), output.ledgerHeader->seq},
        {JS(ledger_hash), ripple::strHex(output.ledgerHeader->hash)},
    };

    if (output.apiVersion > 1u) {
        jv.as_object()[JS(close_time_iso)] = ripple::to_string_iso(output.ledgerHeader->closeTime);
        if (output.tx.contains(JS(hash))) {
            jv.as_object()[JS(hash)] = output.tx.at(JS(hash));
            jv.as_object()[JS(tx_json)].as_object().erase(JS(hash));
        }
    }
}

TransactionEntryHandler::Input
tag_invoke(boost::json::value_to_tag<TransactionEntryHandler::Input>, boost::json::value const& jv)
{
    auto input = TransactionEntryHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.txHash = boost::json::value_to<std::string>(jv.at(JS(tx_hash)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

}  // namespace rpc
