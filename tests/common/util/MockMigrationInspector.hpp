#pragma once

#include "migration/MigrationInspectorInterface.hpp"
#include "migration/MigratiorStatus.hpp"

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

struct MockMigrationInspector : migration::MigrationInspectorInterface {
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
};

using MockMigrationInspectorSharedPtr = std::shared_ptr<testing::NiceMock<MockMigrationInspector>>;
