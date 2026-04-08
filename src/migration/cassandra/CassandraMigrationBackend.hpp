#pragma once

#include "data/CassandraBackend.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "migration/cassandra/impl/CassandraMigrationSchema.hpp"
#include "migration/cassandra/impl/Spec.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace migration::cassandra {

/**
 * @brief The backend for the migration. It is a subclass of the CassandraBackend and provides the
 * migration specific functionalities.
 */
class CassandraMigrationBackend : public data::cassandra::CassandraBackend {
    util::Logger log_{"Migration"};
    data::cassandra::SettingsProvider settingsProvider_;
    impl::CassandraMigrationSchema migrationSchema_;

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
        LOG(log_.debug()) << "Travsering token range: " << start << " - " << end
                          << " ; table: " << TableDesc::kTABLE_NAME;
        // for each table we only have one prepared statement
        static auto kSTATEMENT_PREPARED = migrationSchema_.getPreparedFullScanStatement(
            handle_, TableDesc::kTABLE_NAME, TableDesc::kPARTITION_KEY
        );

        auto const statement = kSTATEMENT_PREPARED.bind(start, end);

        auto const res = this->executor_.read(yield, statement);
        if (not res) {
            LOG(log_.error()) << "Could not fetch data from table: " << TableDesc::kTABLE_NAME
                              << " range: " << start << " - " << end << ";" << res.error();
            return;
        }

        auto const& results = res.value();
        if (not results.hasRows()) {
            LOG(log_.debug()) << "No rows returned  - table: " << TableDesc::kTABLE_NAME
                              << " range: " << start << " - " << end;
            return;
        }

        for (auto const& row : std::apply(
                 [&](auto... args) { return data::cassandra::extract<decltype(args)...>(results); },
                 typename TableDesc::Row{}
             )) {
            callback(row);
        }
    }
};
}  // namespace migration::cassandra
