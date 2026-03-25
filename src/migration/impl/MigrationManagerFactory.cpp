#include "migration/impl/MigrationManagerFactory.hpp"

#include "data/LedgerCacheInterface.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/CassandraMigrationManager.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <string>
#include <utility>

namespace migration::impl {

std::expected<std::shared_ptr<MigrationManagerInterface>, std::string>
makeMigrationManager(
    util::config::ClioConfigDefinition const& config,
    data::LedgerCacheInterface& cache
)
{
    static util::Logger const log{"Migration"};  // NOLINT(readability-identifier-naming)
    LOG(log.info()) << "Constructing MigrationManager";

    auto const type = config.get<std::string>("database.type");

    if (not boost::iequals(type, "cassandra")) {
        LOG(log.error()) << "Unknown database type to migrate: " << type;
        return std::unexpected(std::string("Invalid database type"));
    }

    auto const cfg = config.getObject("database." + type);
    auto migrationCfg = config.getObject("migration");

    return std::make_shared<cassandra::CassandraMigrationManager>(
        std::make_shared<cassandra::CassandraMigrationBackend>(
            data::cassandra::SettingsProvider{cfg}, cache
        ),
        std::move(migrationCfg)
    );
}

}  // namespace migration::impl
