//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "migration/cassandra/impl/ObjectsAdapter.hpp"

#include <boost/asio/spawn.hpp>
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
