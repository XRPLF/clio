#include "util/MockMigrationBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>

struct SimpleTestMigrator {
    using Backend = MockMigrationBackend;
    static constexpr auto kNAME = "SimpleTestMigrator";
    static constexpr auto kDESCRIPTION = "The migrator for version 0 -> 1";
    static constexpr auto kCAN_BLOCK_CLIO = true;

    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};

struct SimpleTestMigrator2 {
    using Backend = MockMigrationBackend;
    static constexpr auto kNAME = "SimpleTestMigrator2";
    static constexpr auto kDESCRIPTION = "The migrator for version 1 -> 2";
    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};

struct SimpleTestMigrator3 {
    using Backend = MockMigrationBackend;
    static constexpr auto kNAME = "SimpleTestMigrator3";
    static constexpr auto kDESCRIPTION = "The migrator for version 3 -> 4";
    static constexpr auto kCAN_BLOCK_CLIO = false;

    static void
    runMigration(std::shared_ptr<MockMigrationBackend>, util::config::ObjectView const&)
    {
    }
};
