#pragma once

#include "migration/MigrationInspectorInterface.hpp"

#include <string>

namespace migration {

/**
 * @brief The interface for the migration manager. The migration application layer will use this
 * interface to run the migrations. Unlike the MigrationInspectorInterface which only provides the
 * status of migration, this interface contains the actual migration running method.
 */
struct MigrationManagerInterface : virtual public MigrationInspectorInterface {
    /**
     * @brief Run the migration according to the given migrator's name
     */
    virtual void
    runMigration(std::string const&) = 0;
};

}  // namespace migration
