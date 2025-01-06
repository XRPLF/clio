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

#pragma once

#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/impl/FullTableScannerAdapterBase.hpp"

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
    // hash, date, ledger_seq, metadata, transaction
    using Row = std::tuple<ripple::uint256, std::uint64_t, std::uint32_t, ripple::Blob, ripple::Blob>;
    static constexpr char const* kPARTITION_KEY = "hash";
    static constexpr char const* kTABLE_NAME = "transactions";
};

/**
 * @brief The adapter for the transactions table. This class is responsible for reading the transactions from the
 * FullTableScanner and converting the blobs to the STTx and TxMeta.
 */
class TransactionsAdapter : public impl::FullTableScannerAdapterBase<TableTransactionsDesc> {
public:
    using OnTransactionRead = std::function<void(ripple::STTx, ripple::TxMeta)>;

    /**
     * @brief Construct a new Transactions Adapter object
     *
     * @param backend The backend
     * @param onTxRead The callback to call when a transaction is read
     */
    explicit TransactionsAdapter(std::shared_ptr<CassandraMigrationBackend> backend, OnTransactionRead onTxRead)
        : FullTableScannerAdapterBase<TableTransactionsDesc>(backend), onTransactionRead_{std::move(onTxRead)}
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
    OnTransactionRead onTransactionRead_;
};

}  // namespace migration::cassandra::impl
