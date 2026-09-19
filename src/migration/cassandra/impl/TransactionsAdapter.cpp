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

    // Catch only deserialization errors (xrpl throws std::runtime_error) and report them via
    // onDecodeFailure_; the owner decides the policy. Other exceptions (e.g. std::bad_alloc)
    // propagate to fail the scan closed in migrateInTokenRange.
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
