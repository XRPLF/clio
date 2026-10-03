#pragma once

#include "util/MockMigrationBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>
#include <stdexcept>

struct SimpleTestMigrator {
    using Backend = MockMigrationBackend;
    static constexpr auto kName = "SimpleTestMigrator";
    static constexpr auto kDescription = "The migrator for version 0 -> 1";
    static constexpr auto kCanBlockClio = true;

    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};

struct SimpleTestMigrator2 {
    using Backend = MockMigrationBackend;
    static constexpr auto kName = "SimpleTestMigrator2";
    static constexpr auto kDescription = "The migrator for version 1 -> 2";
    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};

struct ThrowingTestMigrator {
    using Backend = MockMigrationBackend;
    static constexpr auto kName = "ThrowingTestMigrator";
    static constexpr auto kDescription = "A migrator whose run fails";

    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
        throw std::runtime_error("migration failed");
    }
};

struct SimpleTestMigrator3 {
    using Backend = MockMigrationBackend;
    static constexpr auto kName = "SimpleTestMigrator3";
    static constexpr auto kDescription = "The migrator for version 3 -> 4";
    static constexpr auto kCanBlockClio = false;

    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};
