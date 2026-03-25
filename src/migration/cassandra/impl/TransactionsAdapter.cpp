#include "migration/cassandra/impl/TransactionsAdapter.hpp"

#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

namespace migration::cassandra::impl {

void
TransactionsAdapter::onRowRead(TableTransactionsDesc::Row const& row)
{
    auto const& [txHash, date, ledgerSeq, metaBlob, txBlob] = row;

    ripple::SerialIter it{txBlob.data(), txBlob.size()};
    ripple::STTx const sttx{it};
    ripple::TxMeta const txMeta{sttx.getTransactionID(), ledgerSeq, metaBlob};
    onTransactionRead_(sttx, txMeta);
}
}  // namespace migration::cassandra::impl
