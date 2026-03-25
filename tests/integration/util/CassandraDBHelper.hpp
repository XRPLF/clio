#pragma once

#include "data/cassandra/Types.hpp"

#include <cstdint>
#include <string>

/**
 * @brief Write a transaction to the database from a CSV string.
 *
 * @param space The keyspace to write the transaction to.
 * @param record The CSV string representing the transaction.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeTxFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
);

/**
 * @brief Write an object to the database from a CSV string.
 *
 * @param space The keyspace to write the object to.
 * @param record The CSV string representing the object.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeObjectFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
);

/**
 * @brief Write a ledger to the database from a CSV string.
 *
 * @param space The keyspace to write the ledger to.
 * @param record The CSV string representing the ledger.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeLedgerFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
);

/**
 * @brief Write a range of ledgers to the database.
 *
 * @param space The keyspace to write the ledgers to.
 * @param minSeq The minimum ledger sequence.
 * @param maxSeq The maximum ledger sequence.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeLedgerRange(
    std::string const& space,
    std::uint32_t minSeq,
    std::uint32_t maxSeq,
    data::cassandra::Handle const& handler
);
