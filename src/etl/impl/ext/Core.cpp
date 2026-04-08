#include "etl/impl/ext/Core.hpp"

#include "data/BackendInterface.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace etl::impl {

CoreExt::CoreExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
CoreExt::onLedgerData(model::LedgerData const& data)
{
    LOG(log_.debug()) << "Loading ledger data for " << data.seq;
    backend_->writeLedger(data.header, auto{data.rawHeader});
    insertTransactions(data);
}

void
CoreExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.info()) << "Loading initial ledger data for " << data.seq;
    backend_->writeLedger(data.header, auto{data.rawHeader});
    insertTransactions(data);
}

void
CoreExt::onInitialObject(uint32_t seq, model::Object const& obj)
{
    LOG(log_.trace()) << "got initial OBJ = " << obj.key << " for seq " << seq;
    backend_->writeLedgerObject(auto{obj.keyRaw}, seq, auto{obj.dataRaw});
}

void
CoreExt::onObject(uint32_t seq, model::Object const& obj)
{
    LOG(log_.trace()) << "got OBJ = " << obj.key << " for seq " << seq;
    backend_->writeLedgerObject(auto{obj.keyRaw}, seq, auto{obj.dataRaw});
}

void
CoreExt::insertTransactions(model::LedgerData const& data)
{
    for (auto const& txn : data.transactions) {
        LOG(log_.trace()) << "Inserting transaction = " << txn.sttx.getTransactionID();

        backend_->writeAccountTransaction({txn.meta, txn.sttx.getTransactionID()});
        backend_->writeTransaction(
            auto{txn.key},
            data.seq,
            data.header.closeTime.time_since_epoch()
                .count(),  // This is why we can't use 'onTransaction'
            auto{txn.raw},
            auto{txn.metaRaw}
        );
    }
}

}  // namespace etl::impl
