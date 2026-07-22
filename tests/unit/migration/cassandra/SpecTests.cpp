#include "migration/cassandra/impl/Spec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <tuple>

namespace {
class Empty {};

struct SimpleTestTable {
    using Row = std::tuple<std::uint32_t, std::uint32_t>;
    static constexpr char const* kPartitionKey = "key";
    static constexpr char const* kTableName = "test";
};
}  // namespace
TEST(MigrationSpec, TableSpec)
{
    static_assert(!migration::cassandra::impl::TableSpec<Empty>);
    static_assert(migration::cassandra::impl::TableSpec<SimpleTestTable>);
}
