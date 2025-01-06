//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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

#include "data/cassandra/Handle.hpp"
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
writeTxFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler);

/**
 * @brief Write an object to the database from a CSV string.
 *
 * @param space The keyspace to write the object to.
 * @param record The CSV string representing the object.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeObjectFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler);

/**
 * @brief Write a ledger to the database from a CSV string.
 *
 * @param space The keyspace to write the ledger to.
 * @param record The CSV string representing the ledger.
 * @param handler The Cassandra database handler.
 * @return The result of the operation.
 */
data::cassandra::ResultOrError
writeLedgerFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler);

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
