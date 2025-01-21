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

#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/core.h>
#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Test backend for Cassandra migration. The class is mainly to provide an example of how to add the needed
 * backend for the migrator. It is used in integration tests to provide the backend for the example migrators. In
 * production, the backend code should be added to CassandraMigrationBackend directly.
 */
class CassandraMigrationTestBackend : public migration::cassandra::CassandraMigrationBackend {
    data::cassandra::SettingsProvider settingsProvider_;

public:
    /**
     * @brief Construct a new Cassandra Migration Test Backend object
     *
     * @param settingsProvider The settings provider for the Cassandra backend
     */
    CassandraMigrationTestBackend(data::cassandra::SettingsProvider settingsProvider)
        : migration::cassandra::CassandraMigrationBackend(settingsProvider)
        , settingsProvider_(std::move(settingsProvider))

    {
        if (auto const res = handle_.executeEach(createTablesSchema()); not res)
            throw std::runtime_error("Could not create schema: " + res.error());
    }

    /**
     * @brief Write a transaction hash and its transaction type to the tx_index_example table. It's used by
     * ExampleTransactionsMigrator.
     *
     * @param hash The transaction hash
     * @param txType The transaction type
     */
    void
    writeTxIndexExample(std::string const& hash, std::string const& txType)
    {
        auto static kINSERT_TX_INDEX_EXAMPLE = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (hash, tx_type)
                VALUES (?, ?)
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "tx_index_example")
            ));
        }();
        executor_.writeSync(kINSERT_TX_INDEX_EXAMPLE.bind(hash, data::cassandra::Text(txType)));
    }

    /**
     * @brief Fetch the transaction type via transaction hash from the tx_index_example table. It's used by
     * ExampleTransactionsMigrator validation.
     *
     * @param hash The transaction hash
     * @param ctx The boost asio context
     * @return The transaction type if found, otherwise std::nullopt
     */
    std::optional<std::string>
    fetchTxTypeViaID(std::string const& hash, boost::asio::yield_context ctx)
    {
        auto static kFETCH_TX_TYPE = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                SELECT tx_type FROM {} WHERE hash = ?
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "tx_index_example")
            ));
        }();
        auto const res = executor_.read(ctx, kFETCH_TX_TYPE.bind(hash));
        if (not res) {
            return std::nullopt;
        }

        auto const& result = res.value();
        if (not result.hasRows()) {
            return std::nullopt;
        }

        for (auto const& [txType] : data::cassandra::extract<std::string>(result)) {
            return txType;
        }
        return std::nullopt;
    }

    /**
     * @brief Fetch the transaction index table size. It's used by ExampleTransactionsMigrator validation.
     *
     * @param ctx The boost asio context
     * @return The size of the transaction index table if found, otherwise std::nullopt
     */
    std::optional<std::uint64_t>
    fetchTxIndexTableSize(boost::asio::yield_context ctx)
    {
        auto static kINSERT_TX_INDEX_EXAMPLE = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                SELECT COUNT(*) FROM {}
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "tx_index_example")
            ));
        }();

        // This function will be called after table being dropped, catch the exception
        try {
            auto const res = executor_.read(ctx, kINSERT_TX_INDEX_EXAMPLE);
            if (not res) {
                return std::nullopt;
            }

            auto const& result = res.value();
            if (not result.hasRows()) {
                return std::nullopt;
            }

            for (auto const& [size] : data::cassandra::extract<std::uint64_t>(result)) {
                return size;
            }
        } catch (std::exception& e) {
            return std::nullopt;
        }
        return std::nullopt;
    }

    /**
     *@brief Write the ledger account hash to the ledger_example table. It's used by ExampleLedgerMigrator.
     *
     * @param sequence The ledger sequence
     * @param accountHash The account hash
     */
    void
    writeLedgerAccountHash(std::uint64_t sequence, std::string const& accountHash)
    {
        auto static kINSERT_LEDGER_EXAMPLE = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (sequence, account_hash)
                VALUES (?, ?)
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "ledger_example")
            ));
        }();
        executor_.writeSync(kINSERT_LEDGER_EXAMPLE.bind(sequence, accountHash));
    }

    /**
     * @brief Fetch the account hash via ledger sequence from the ledger_example table. It's used by
     * ExampleLedgerMigrator validation.
     *
     * @param sequence The ledger sequence
     * @param ctx The boost asio context
     * @return The account hash if found, otherwise std::nullopt
     */
    std::optional<ripple::uint256>
    fetchAccountHashViaSequence(std::uint64_t sequence, boost::asio::yield_context ctx)
    {
        auto static kFETCH_ACCOUNT_HASH = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                SELECT account_hash FROM {} WHERE sequence = ?
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "ledger_example")
            ));
        }();
        auto const res = executor_.read(ctx, kFETCH_ACCOUNT_HASH.bind(sequence));
        if (not res) {
            return std::nullopt;
        }

        auto const& result = res.value();
        if (not result.hasRows()) {
            return std::nullopt;
        }

        for (auto const& [accountHash] : data::cassandra::extract<ripple::uint256>(result)) {
            return accountHash;
        }
        return std::nullopt;
    }

    /**
     * @brief Fetch the ledger example table size. It's used by ExampleLedgerMigrator validation.
     *
     * @param ctx The boost asio context
     * @return The size of the ledger example table if found, otherwise std::nullopt
     */
    std::optional<std::uint64_t>
    fetchLedgerTableSize(boost::asio::yield_context ctx)
    {
        auto static kINSERT_LEDGER_EXAMPLE = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                SELECT COUNT(*) FROM {}
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "ledger_example")
            ));
        }();

        // This function will be called after table being dropped, catch the exception
        try {
            auto const res = executor_.read(ctx, kINSERT_LEDGER_EXAMPLE);
            if (not res) {
                return std::nullopt;
            }

            auto const& result = res.value();
            if (not result.hasRows()) {
                return std::nullopt;
            }

            for (auto const& [size] : data::cassandra::extract<std::uint64_t>(result)) {
                return size;
            }
        } catch (std::exception& e) {
            return std::nullopt;
        }
        return std::nullopt;
    }

    /**
     * @brief Drop the diff table. It's used by ExampleDropTableMigrator.
     *
     * @return The result of the operation
     */
    auto
    dropDiffTable()
    {
        return handle_.execute(fmt::format(
            R"(
            DROP TABLE IF EXISTS {}
            )",
            data::cassandra::qualifiedTableName(settingsProvider_, "diff")
        ));
    }

    /**
     * @brief Fetch the diff table size. It's used by ExampleDropTableMigrator validation.
     *
     * @param ctx The boost asio context
     * @return The size of the diff table if found, otherwise std::nullopt
     */
    std::optional<std::uint64_t>
    fetchDiffTableSize(boost::asio::yield_context ctx)
    {
        auto static kCOUNT_DIFF = [this]() {
            return handle_.prepare(fmt::format(
                R"(
                SELECT COUNT(*) FROM {}
                )",
                data::cassandra::qualifiedTableName(settingsProvider_, "diff")
            ));
        }();

        // This function will be called after table being dropped, catch the exception
        try {
            auto const res = executor_.read(ctx, kCOUNT_DIFF);
            if (not res) {
                return std::nullopt;
            }

            auto const& result = res.value();
            if (not result.hasRows()) {
                return std::nullopt;
            }

            for (auto const& [size] : data::cassandra::extract<std::uint64_t>(result)) {
                return size;
            }
        } catch (std::exception& e) {
            return std::nullopt;
        }
        return std::nullopt;
    }

private:
    std::vector<data::cassandra::Statement>
    createTablesSchema()
    {
        std::vector<data::cassandra::Statement> statements;

        statements.emplace_back(fmt::format(
            R"(
            CREATE TABLE IF NOT EXISTS {}
                   (      
                        hash blob,
                     tx_type text,
                     PRIMARY KEY (hash) 
                   ) 
            )",
            data::cassandra::qualifiedTableName(settingsProvider_, "tx_index_example")
        ));

        statements.emplace_back(fmt::format(
            R"(
            CREATE TABLE IF NOT EXISTS {}
                   (      
                        sequence bigint,
                    account_hash blob,
                         PRIMARY KEY (sequence) 
                   ) 
            )",
            data::cassandra::qualifiedTableName(settingsProvider_, "ledger_example")
        ));
        return statements;
    }
};
