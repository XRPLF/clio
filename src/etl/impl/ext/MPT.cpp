#include "etl/impl/ext/MPT.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "etl/MPTHelpers.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/strHex.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace etl::impl {

MPTExt::MPTExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
MPTExt::onLedgerData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got TXS cnt = " << data.transactions.size()
                      << "; OBJS size = " << data.objects.size();
    writeMPTHoldersFromTransactions(data);
}

void
MPTExt::onInitialObject(uint32_t, model::Object const& obj)
{
    LOG(log_.trace()) << "got initial object with key: " << ripple::strHex(obj.key);
    if (auto const mptHolder = getMPTHolderFromObj(obj.keyRaw, obj.dataRaw); mptHolder.has_value())
        backend_->writeMPTHolders({*mptHolder});
}

void
MPTExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got initial TXS cnt = " << data.transactions.size();
    writeMPTHoldersFromTransactions(data);
}

void
MPTExt::writeMPTHoldersFromTransactions(model::LedgerData const& data)
{
    std::vector<MPTHolderData> holders;

    for (auto const& tx : data.transactions) {
        auto const mptHolders = getMPTHolderFromTx(tx.meta, tx.sttx);
        holders.append_range(mptHolders);
    }

    if (not holders.empty())
        backend_->writeMPTHolders(holders);
}

}  // namespace etl::impl
