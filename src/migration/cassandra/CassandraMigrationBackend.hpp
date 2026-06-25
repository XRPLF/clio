#pragma once

#include "data/CassandraBackend.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "migration/cassandra/impl/CassandraMigrationSchema.hpp"
#include "migration/cassandra/impl/Spec.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/core.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace migration::cassandra {

/**
 * @brief The backend for the migration. It is a subclass of the CassandraBackend and provides the
 * migration specific functionalities.
 */
class CassandraMigrationBackend : public data::cassandra::CassandraBackend {
    // Internal full-scan page size: large enough to avoid excessive driver round trips, bounded so
    // one migration read cannot materialize an unbounded token range page. Operators still control
    // concurrency through the existing migration thread/job settings.
    static constexpr std::int32_t kFullScanPageSize = 5'000;

    util::Logger log_{"Migration"};
    data::cassandra::SettingsProvider settingsProvider_;
    impl::CassandraMigrationSchema migrationSchema_;
    std::mutex fullScanStatementsMutex_;
    std::unordered_map<std::string, std::shared_ptr<data::cassandra::PreparedStatement>>
        fullScanStatements_;

    template <impl::TableSpec TableDesc>
    std::shared_ptr<data::cassandra::PreparedStatement>
    getPreparedFullScanStatement()
    {
        auto const statementKey = fmt::format(
            "{}:{}:{}", TableDesc::kTableName, TableDesc::kPartitionKey, TableDesc::kSelectColumns
        );

        std::scoped_lock const lock{fullScanStatementsMutex_};
        if (auto const statement = fullScanStatements_.find(statementKey);
            statement != fullScanStatements_.end()) {
            return statement->second;
        }

        auto statement = std::make_shared<data::cassandra::PreparedStatement>(
            migrationSchema_.getPreparedFullScanStatement(
                handle_, TableDesc::kTableName, TableDesc::kSelectColumns, TableDesc::kPartitionKey
            )
        );
        return fullScanStatements_.emplace(statementKey, std::move(statement)).first->second;
    }

public:
    /**
     * @brief Construct a new Cassandra Migration Backend object. The backend is not readonly.
     *
     * @param settingsProvider The settings provider
     * @param cache The ledger cache to use
     */
    explicit CassandraMigrationBackend(
        data::cassandra::SettingsProvider settingsProvider,
        data::LedgerCacheInterface& cache
    )
        : data::cassandra::CassandraBackend{auto{settingsProvider}, cache, false /* not readonly */}
        , settingsProvider_(std::move(settingsProvider))
        , migrationSchema_{settingsProvider_}
    {
    }

    /**
     *@brief Scan a table in a token range and call the callback for each row
     *
     *@tparam TableDesc The table description of the table to scan
     *@param start The start token
     *@param end The end token
     *@param callback The callback to call for each row
     *@param yield The boost asio yield context
     */
    template <impl::TableSpec TableDesc>
    void
    migrateInTokenRange(
        std::int64_t const start,
        std::int64_t const end,
        auto const& callback,
        boost::asio::yield_context yield
    )
    {
        LOG(log_.debug()) << "Traversing token range: " << start << " - " << end
                          << " ; table: " << TableDesc::kTableName;

        auto const statementPrepared = getPreparedFullScanStatement<TableDesc>();
        auto statement = statementPrepared->bind(start, end);
        statement.setPagingSize(kFullScanPageSize);

        std::uint64_t rowsRead = 0;
        while (true) {
            auto const res = this->executor_.read(yield, statement);
            if (not res) {
                // Fail closed: a swallowed read error would leave a gap in the scanned data while
                // the migrator is still marked Migrated. Throwing aborts the migration so its
                // status stays NotMigrated and the operator can rerun.
                LOG(log_.error()) << "Could not fetch data from table: " << TableDesc::kTableName
                                  << " range: " << start << " - " << end << ";" << res.error();
                throw std::runtime_error(
                    fmt::format(
                        "Migration scan failed to read table '{}' in token range [{}, {}]: {}",
                        TableDesc::kTableName,
                        start,
                        end,
                        res.error().message()
                    )
                );
            }

            auto const& results = res.value();
            for (auto const& row : std::apply(
                     [&](auto... args) {
                         return data::cassandra::extract<decltype(args)...>(results);
                     },
                     typename TableDesc::Row{}
                 )) {
                callback(row);
                ++rowsRead;
            }

            if (not results.hasMorePages())
                break;

            statement.setPagingState(results);
        }

        if (rowsRead == 0) {
            LOG(log_.debug()) << "No rows returned  - table: " << TableDesc::kTableName
                              << " range: " << start << " - " << end;
        }
    }
};
}  // namespace migration::cassandra
