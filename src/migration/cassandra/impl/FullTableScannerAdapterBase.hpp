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
#include "migration/cassandra/impl/FullTableScanner.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <utility>

namespace migration::cassandra::impl {

/**
 * @brief The base class for the full table scanner adapter. It is responsible for reading the rows from the full table
 * scanner and call the callback when a row is read. With this base class, each table adapter can focus on the actual
 * row data converting.
 *
 * @tparam TableDesc The table description, it has to be a TableSpec.
 */
template <TableSpec TableDesc>
struct FullTableScannerAdapterBase {
    static_assert(TableSpec<TableDesc>);

protected:
    /**
     * @brief The backend to use
     */
    std::shared_ptr<CassandraMigrationBackend> backend_;

public:
    virtual ~FullTableScannerAdapterBase() = default;

    /**
     * @brief Construct a new Full Table Scanner Adapter Base object
     *
     * @param backend The backend
     */
    FullTableScannerAdapterBase(std::shared_ptr<CassandraMigrationBackend> backend) : backend_(std::move(backend))
    {
    }

    /**
     * @brief Read the row in the given token range from database, this is the adapt function for the FullTableScanner.
     *
     * @param range The token range to read
     * @param yield The yield context
     */
    void
    readByTokenRange(TokenRange const& range, boost::asio::yield_context yield)
    {
        backend_->migrateInTokenRange<TableDesc>(
            range.start, range.end, [this](auto const& row) { onRowRead(row); }, yield
        );
    }

    /**
     * @brief Called when a row is read. The derived class should implement this function to convert the database blob
     * to actual data type.
     *
     * @param row The row read
     */
    virtual void
    onRowRead(TableDesc::Row const& row) = 0;
};
}  // namespace migration::cassandra::impl
