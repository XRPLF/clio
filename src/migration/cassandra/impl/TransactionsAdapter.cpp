#include "migration/cassandra/impl/TransactionsAdapter.hpp"

#include "util/log/Logger.hpp"

#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <exception>
#include <optional>

namespace migration::cassandra::impl {

void
TransactionsAdapter::onRowRead(TableTransactionsDesc::Row const& row)
{
    auto const& [txHash, date, ledgerSeq, metaBlob, txBlob] = row;

    // A transaction that fails to deserialize is a deterministic data error: rerunning the
    // migration would hit it again forever. Log and skip the row instead of wedging the whole
    // scan; genuine infrastructure failures (read errors) still fail closed in
    // migrateInTokenRange.
    std::optional<xrpl::STTx> sttx;
    std::optional<xrpl::TxMeta> txMeta;
    try {
        xrpl::SerialIter it{txBlob.data(), txBlob.size()};
        sttx.emplace(it);
        txMeta.emplace(sttx->getTransactionID(), ledgerSeq, metaBlob);
    } catch (std::exception const& e) {
        LOG(log_.error()) << "Skipping transaction that failed to deserialize: hash "
                          << xrpl::strHex(txHash) << ", ledger " << ledgerSeq << ": " << e.what();
        return;
    }

    onTransactionRead_(*sttx, *txMeta);
}
}  // namespace migration::cassandra::impl
