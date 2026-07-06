#include "etl/impl/ext/MPT.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "etl/MPTHelpers.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/strHex.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
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
    writeMPTDataFromTransactions(data);
}

void
MPTExt::onInitialObject(uint32_t, model::Object const& obj)
{
    LOG(log_.trace()) << "got initial object with key: " << xrpl::strHex(obj.key);
    if (auto const mptHolder = getMPTHolderFromObj(obj.keyRaw, obj.dataRaw); mptHolder.has_value())
        backend_->writeMPTHolders({*mptHolder});
}

void
MPTExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got initial TXS cnt = " << data.transactions.size();
    writeMPTDataFromTransactions(data);
}

void
MPTExt::writeMPTDataFromTransactions(model::LedgerData const& data)
{
    std::vector<MPTHolderData> holders;
    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::size_t indexRowsWritten = 0;
    static constexpr std::size_t kIndexRowsPerTxWarningThreshold = 1000;

    for (auto const& tx : data.transactions) {
        auto const mptHolders = getMPTHolderFromTx(tx.meta, tx.sttx);
        holders.append_range(mptHolders);

        auto txIndexData = getMPTokenIssuanceTxsFromTx(tx.meta, tx.sttx);

        std::size_t txIndexRows = 0;
        for (auto const& record : txIndexData)
            txIndexRows += 1 + record.accounts.size();

        if (txIndexRows > kIndexRowsPerTxWarningThreshold) {
            LOG(log_.warn()) << "MPT issuance tx index fanout of " << txIndexRows
                             << " rows exceeds the expected bound of "
                             << kIndexRowsPerTxWarningThreshold << " for tx "
                             << xrpl::strHex(tx.id);
        }

        indexRowsWritten += txIndexRows;
        issuanceTxs.insert(
            issuanceTxs.end(),
            std::make_move_iterator(txIndexData.begin()),
            std::make_move_iterator(txIndexData.end())
        );
    }

    if (not holders.empty())
        backend_->writeMPTHolders(holders);

    if (not issuanceTxs.empty()) {
        backend_->writeMPTokenIssuanceTransactions(issuanceTxs);
        backend_->writeAccountMPTokenIssuanceTransactions(issuanceTxs);
        issuanceTxIndexRowsWritten_.get() += static_cast<std::uint64_t>(indexRowsWritten);
    }
}

}  // namespace etl::impl
