#include "migration/cassandra/impl/TransactionsAdapter.hpp"

#include "util/log/Logger.hpp"

#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <optional>
#include <stdexcept>

namespace migration::cassandra::impl {

void
TransactionsAdapter::onRowRead(TableTransactionsDesc::Row const& row)
{
    auto const& [txHash, date, ledgerSeq, metaBlob, txBlob] = row;

    // The transactions table is written by Clio's own ETL from validated ledgers, so a row that
    // cannot be parsed signals storage corruption or a code bug (e.g. a schema/column change) --
    // not a benign condition. Catch only deserialization errors (xrpl throws std::runtime_error for
    // these) and report them to the owner via onDecodeFailure_; the owner decides the policy
    // (count, threshold, abort). Anything else -- notably std::bad_alloc from the allocation-heavy
    // parse -- is a transient infrastructure failure that must propagate and fail the scan closed
    // in migrateInTokenRange rather than be mistaken for a bad row.
    std::optional<xrpl::STTx> sttx;
    std::optional<xrpl::TxMeta> txMeta;
    try {
        xrpl::SerialIter it{txBlob.data(), txBlob.size()};
        sttx.emplace(it);
        txMeta.emplace(sttx->getTransactionID(), ledgerSeq, metaBlob);
    } catch (std::runtime_error const& e) {
        LOG(log_.error()) << "Failed to deserialize transaction: hash " << xrpl::strHex(txHash)
                          << ", ledger " << ledgerSeq << ": " << e.what();
        onDecodeFailure_();
        return;
    }

    onTransactionRead_(*sttx, *txMeta);
}
}  // namespace migration::cassandra::impl
