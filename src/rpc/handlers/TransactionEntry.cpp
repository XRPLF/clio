#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doTransactionEntry(Context const& context)
{
    boost::json::object response;
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::uint256 hash;
    if (!hash.parseHex(getRequiredString(context.params, "tx_hash")))
        return Status{Error::rpcINVALID_PARAMS, "malformedTransaction"};

    auto dbResponse = context.backend->fetchTransaction(hash);
    // Note: transaction_entry is meant to only search a specified ledger for
    // the specified transaction. tx searches the entire range of history. For
    // rippled, having two separate commands made sense, as tx would use SQLite
    // and transaction_entry used the nodestore. For clio though, there is no
    // difference between the implementation of these two, as clio only stores
    // transactions in a transactions table, where the key is the hash. However,
    // the API for transaction_entry says the method only searches the specified
    // ledger; we simulate that here by returning not found if the transaction
    // is in a different ledger than the one specified.
    if (!dbResponse || dbResponse->ledgerSequence != lgrInfo.seq)
        return Status{
            Error::rpcTXN_NOT_FOUND,
            "transactionNotFound",
            "Transaction not found."};

    auto [txn, meta] = toExpandedJson(*dbResponse);
    response["tx_json"] = std::move(txn);
    response["metadata"] = std::move(meta);
    response["ledger_index"] = lgrInfo.seq;
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    return response;
}

}  // namespace RPC
