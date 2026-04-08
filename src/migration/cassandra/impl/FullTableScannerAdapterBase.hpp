#pragma once

#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/impl/FullTableScanner.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <utility>

namespace migration::cassandra::impl {

/**
 * @brief The base class for the full table scanner adapter. It is responsible for reading the rows
 * from the full table scanner and call the callback when a row is read. With this base class, each
 * table adapter can focus on the actual row data converting.
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
    FullTableScannerAdapterBase(std::shared_ptr<CassandraMigrationBackend> backend)
        : backend_(std::move(backend))
    {
    }

    /**
     * @brief Read the row in the given token range from database, this is the adapt function for
     * the FullTableScanner.
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
     * @brief Called when a row is read. The derived class should implement this function to convert
     * the database blob to actual data type.
     *
     * @param row The row read
     */
    virtual void
    onRowRead(TableDesc::Row const& row) = 0;
};
}  // namespace migration::cassandra::impl
