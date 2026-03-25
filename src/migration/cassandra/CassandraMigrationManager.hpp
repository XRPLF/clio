#pragma once

#include "data/BackendInterface.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/impl/MigrationInspectorBase.hpp"
#include "migration/impl/MigrationManagerBase.hpp"
#include "migration/impl/MigratorsRegister.hpp"

namespace {

// Register migrators here
// MigratorsRegister<BackendType, ExampleMigrator>
template <typename BackendType>
using CassandraSupportedMigrators = migration::impl::MigratorsRegister<BackendType>;

//  Instantiates with the backend which supports actual migration running
using MigrationProcessor =
    CassandraSupportedMigrators<migration::cassandra::CassandraMigrationBackend>;

// Instantiates with backend interface, it doesn't support actual migration. But it can be used to
// inspect the migrators status
using MigrationQuerier = CassandraSupportedMigrators<data::BackendInterface>;

}  // namespace

namespace migration::cassandra {

using CassandraMigrationInspector = migration::impl::MigrationInspectorBase<MigrationQuerier>;

using CassandraMigrationManager = migration::impl::MigrationManagerBase<MigrationProcessor>;

}  // namespace migration::cassandra
