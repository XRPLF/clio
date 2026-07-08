#pragma once

#include "migration/MigrationManagerInterface.hpp"
#include "migration/MigratiorStatus.hpp"

#include <gmock/gmock.h>

#include <string>
#include <tuple>
#include <vector>

struct MockMigrationManager : public migration::MigrationManagerInterface {
    MOCK_METHOD(
        (std::vector<std::tuple<std::string, migration::MigratorStatus>>),
        allMigratorsStatusPairs,
        (),
        (const, override)
    );
    MOCK_METHOD(std::vector<std::string>, allMigratorsNames, (), (const, override));
    MOCK_METHOD(
        migration::MigratorStatus,
        getMigratorStatusByName,
        (std::string const&),
        (const, override)
    );
    MOCK_METHOD(std::string, getMigratorDescriptionByName, (std::string const&), (const, override));
    MOCK_METHOD(bool, isBlockingClio, (), (const, override));
    MOCK_METHOD(void, runMigration, (std::string const&), (override));
};
