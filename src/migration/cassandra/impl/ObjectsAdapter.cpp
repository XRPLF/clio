#include "migration/cassandra/impl/ObjectsAdapter.hpp"

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>

#include <optional>
#include <utility>

namespace migration::cassandra::impl {

void
ObjectsAdapter::onRowRead(TableObjectsDesc::Row const& row)
{
    auto const& [key, ledgerSeq, blob] = row;
    // the blob can be empty which means the ledger state is deleted
    if (blob.empty()) {
        onStateRead_(ledgerSeq, std::nullopt);
        return;
    }
    ripple::SLE sle{ripple::SerialIter{blob.data(), blob.size()}, key};
    onStateRead_(ledgerSeq, std::make_optional(std::move(sle)));
}

}  // namespace migration::cassandra::impl
