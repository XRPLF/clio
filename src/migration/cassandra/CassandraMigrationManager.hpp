//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
using MigrationProcesser = CassandraSupportedMigrators<migration::cassandra::CassandraMigrationBackend>;

// Instantiates with backend interface, it doesn't support actual migration. But it can be used to inspect the migrators
// status
using MigrationQuerier = CassandraSupportedMigrators<data::BackendInterface>;

}  // namespace

namespace migration::cassandra {

using CassandraMigrationInspector = migration::impl::MigrationInspectorBase<MigrationQuerier>;

using CassandraMigrationManager = migration::impl::MigrationManagerBase<MigrationProcesser>;

}  // namespace migration::cassandra
