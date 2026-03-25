#pragma once

#include "migration/cassandra/impl/FullTableScanner.hpp"
#include "migration/cassandra/impl/ObjectsAdapter.hpp"
#include "migration/cassandra/impl/TransactionsAdapter.hpp"

namespace migration::cassandra::impl {

using ObjectsScanner = impl::FullTableScanner<impl::ObjectsAdapter>;
using TransactionsScanner = impl::FullTableScanner<impl::TransactionsAdapter>;

}  // namespace migration::cassandra::impl
