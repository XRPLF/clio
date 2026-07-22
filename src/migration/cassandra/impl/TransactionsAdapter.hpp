#pragma once

#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/impl/FullTableScannerAdapterBase.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

namespace migration::cassandra::impl {

/**
 * @brief The description of the transactions table. It has to be a TableSpec.
 */
struct TableTransactionsDesc {
    // Must match kSelectColumns order.
    using Row = std::tuple<xrpl::uint256, std::uint64_t, std::uint32_t, xrpl::Blob, xrpl::Blob>;
    static constexpr char const* kPartitionKey = "hash";
    static constexpr char const* kSelectColumns =
        "hash, date, ledger_sequence, metadata, transaction";
    static constexpr char const* kTableName = "transactions";
};

/**
 * @brief The adapter for the transactions table. This class is responsible for reading the
 * transactions from the FullTableScanner and deserializing them into STTx and TxMeta.
 */
class TransactionsAdapter : public impl::FullTableScannerAdapterBase<TableTransactionsDesc> {
public:
    using OnTransactionRead = std::function<void(xrpl::STTx const&, xrpl::TxMeta const&)>;
    using OnDecodeFailure = std::function<void()>;

    /**
     * @brief Construct a new Transactions Adapter object
     *
     * @param backend The backend
     * @param onTxRead The callback to call when a transaction is read
     * @param onDecodeFailure The callback to call when a transaction fails to deserialize; the
     * owner decides the policy (count, abort, etc.). Invoked concurrently across scan workers.
     */
    explicit TransactionsAdapter(
        std::shared_ptr<CassandraMigrationBackend> backend,
        OnTransactionRead onTxRead,
        OnDecodeFailure onDecodeFailure
    )
        : FullTableScannerAdapterBase<TableTransactionsDesc>(backend)
        , onTransactionRead_{std::move(onTxRead)}
        , onDecodeFailure_{std::move(onDecodeFailure)}
    {
    }

    /**
     *@brief The callback when a row is read.
     *
     *@param row The row to read
     */
    void
    onRowRead(TableTransactionsDesc::Row const& row) override;

private:
    util::Logger log_{"Migration"};
    OnTransactionRead onTransactionRead_;
    OnDecodeFailure onDecodeFailure_;
};

}  // namespace migration::cassandra::impl
