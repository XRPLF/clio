//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etlng/impl/ext/Core.hpp"

#include "data/BackendInterface.hpp"
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace etlng::impl {

CoreExt::CoreExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
CoreExt::onLedgerData(model::LedgerData const& data) const
{
    LOG(log_.debug()) << "Loading ledger data for " << data.seq;
    backend_->writeLedger(data.header, auto{data.rawHeader});
    insertTransactions(data);
}

void
CoreExt::onInitialData(model::LedgerData const& data) const
{
    LOG(log_.info()) << "Loading initial ledger data for " << data.seq;
    backend_->writeLedger(data.header, auto{data.rawHeader});
    insertTransactions(data);
}

void
CoreExt::onInitialObject(uint32_t seq, model::Object const& obj) const
{
    LOG(log_.trace()) << "got initial OBJ = " << obj.key << " for seq " << seq;
    backend_->writeLedgerObject(auto{obj.keyRaw}, seq, auto{obj.dataRaw});
}

void
CoreExt::onObject(uint32_t seq, model::Object const& obj) const
{
    LOG(log_.trace()) << "got OBJ = " << obj.key << " for seq " << seq;
    backend_->writeLedgerObject(auto{obj.keyRaw}, seq, auto{obj.dataRaw});
}

void
CoreExt::insertTransactions(model::LedgerData const& data) const
{
    for (auto const& txn : data.transactions) {
        LOG(log_.trace()) << "Inserting transaction = " << txn.sttx.getTransactionID();

        backend_->writeAccountTransaction({txn.meta, txn.sttx.getTransactionID()});
        backend_->writeTransaction(
            auto{txn.key},
            data.seq,
            data.header.closeTime.time_since_epoch().count(),  // This is why we can't use 'onTransaction'
            auto{txn.raw},
            auto{txn.metaRaw}
        );
    }
}

}  // namespace etlng::impl
