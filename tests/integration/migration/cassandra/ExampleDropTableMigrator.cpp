#include "migration/cassandra/ExampleDropTableMigrator.hpp"

#include "util/config/ObjectView.hpp"

#include <memory>

void
ExampleDropTableMigrator::runMigration(
    std::shared_ptr<Backend> const& backend,
    util::config::ObjectView const&
)
{
    backend->dropDiffTable();
}
