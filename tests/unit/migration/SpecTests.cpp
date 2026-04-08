#include "migration/TestMigrators.hpp"
#include "migration/impl/Spec.hpp"
#include "util/MockMigrationBackend.hpp"

#include <gtest/gtest.h>

namespace {
class Fake {};

}  // namespace

TEST(MigrationSpec, MigratorSpec)
{
    static_assert(!migration::impl::MigratorSpec<Fake, MockMigrationBackend>);
    static_assert(migration::impl::MigratorSpec<SimpleTestMigrator, MockMigrationBackend>);
}

TEST(MigrationSpec, AllMigratorSpec)
{
    static_assert(!migration::impl::AllMigratorSpec<SimpleTestMigrator, SimpleTestMigrator2, Fake>);
    static_assert(migration::impl::AllMigratorSpec<SimpleTestMigrator2, SimpleTestMigrator>);
}
