//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <backend/cassandra/Concepts.h>
#include <backend/cassandra/Handle.h>
#include <backend/cassandra/SettingsProvider.h>
#include <backend/cassandra/Types.h>
#include <config/Config.h>
#include <log/Logger.h>
#include <util/Expected.h>

#include <fmt/compile.h>

namespace Backend::Cassandra {

template <SomeSettingsProvider SettingsProviderType>
[[nodiscard]] std::string inline qualifiedTableName(SettingsProviderType const& provider, std::string_view name)
{
    return fmt::format("{}.{}{}", provider.getKeyspace(), provider.getTablePrefix().value_or(""), name);
}

/**
 * @brief Manages the DB schema and provides access to prepared statements
 */
template <SomeSettingsProvider SettingsProviderType>
class Schema
{
    // Current schema version.
    // Update this everytime you update the schema.
    // Migrations will be ran automatically based on this value.
    static constexpr uint16_t version = 1u;

    clio::Logger log_{"Backend"};
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;

public:
    explicit Schema(SettingsProviderType const& settingsProvider) : settingsProvider_{std::cref(settingsProvider)}
    {
    }

    std::string createKeyspace = [this]() {
        return fmt::format(
            R"(
            CREATE KEYSPACE IF NOT EXISTS {} 
              WITH replication = {{
                     'class': 'SimpleStrategy',
                     'replication_factor': '{}'
                   }} 
               AND durable_writes = true
            )",
            settingsProvider_.get().getKeyspace(),
            settingsProvider_.get().getReplicationFactor());
    }();

    // =======================
    // Schema creation queries
    // =======================

    std::vector<Statement> createSchema = [this]() {
        std::vector<Statement> statements;

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  (      
                         key blob, 
                    sequence bigint, 
                      object blob, 
                     PRIMARY KEY (key, sequence) 
                  ) 
             WITH CLUSTERING ORDER BY (sequence DESC) 
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "objects"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  (     
                        hash blob PRIMARY KEY, 
             ledger_sequence bigint, 
                        date bigint,
                 transaction blob, 
                    metadata blob 
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "transactions"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  (     
             ledger_sequence bigint, 
                        hash blob, 
                     PRIMARY KEY (ledger_sequence, hash) 
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "ledger_transactions"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  (     
                    key blob,
                    seq bigint, 
                   next blob, 
                PRIMARY KEY (key, seq) 
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "successor"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  (     
                    seq bigint, 
                    key blob,
                PRIMARY KEY (seq, key) 
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "diff"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    account blob,    
                    seq_idx tuple<bigint, bigint>, 
                       hash blob,
                    PRIMARY KEY (account, seq_idx) 
                  ) 
             WITH CLUSTERING ORDER BY (seq_idx DESC)
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "account_tx"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    sequence bigint PRIMARY KEY,
                      header blob
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "ledgers"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    hash blob PRIMARY KEY,
                sequence bigint
                  ) 
             WITH default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "ledger_hashes"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    is_latest boolean PRIMARY KEY,
                     sequence bigint
                  )
            )",
            qualifiedTableName(settingsProvider_.get(), "ledger_range")));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    token_id blob,    
                    sequence bigint,
                       owner blob,
                   is_burned boolean,
                     PRIMARY KEY (token_id, sequence) 
                  ) 
             WITH CLUSTERING ORDER BY (sequence DESC)
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "nf_tokens"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
            CREATE TABLE IF NOT EXISTS {}
                ( 
                    issuer blob,
                    taxon bigint,
                    token_id blob,
                     PRIMARY KEY (issuer, taxon, token_id)
                  ) 
             WITH CLUSTERING ORDER BY (taxon ASC, token_id ASC)
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    token_id blob,
                    sequence bigint,
                         uri blob,
                     PRIMARY KEY (token_id, sequence)
                  ) 
             WITH CLUSTERING ORDER BY (sequence DESC)
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "nf_token_uris"),
            settingsProvider_.get().getTtl()));

        statements.emplace_back(fmt::format(
            R"(
           CREATE TABLE IF NOT EXISTS {}
                  ( 
                    token_id blob,    
                     seq_idx tuple<bigint, bigint>,
                        hash blob,
                     PRIMARY KEY (token_id, seq_idx) 
                  ) 
             WITH CLUSTERING ORDER BY (seq_idx DESC)
              AND default_time_to_live = {}
            )",
            qualifiedTableName(settingsProvider_.get(), "nf_token_transactions"),
            settingsProvider_.get().getTtl()));

        return statements;
    }();

    /**
     * @brief Prepared statements holder
     */
    class Statements
    {
        std::reference_wrapper<SettingsProviderType const> settingsProvider_;
        std::reference_wrapper<Handle const> handle_;

    public:
        Statements(SettingsProviderType const& settingsProvider, Handle const& handle)
            : settingsProvider_{settingsProvider}, handle_{std::cref(handle)}
        {
        }

        //
        // Insert queries
        //

        PreparedStatement insertObject = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (key, sequence, object)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "objects")));
        }();

        PreparedStatement insertTransaction = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (hash, ledger_sequence, date, transaction, metadata)
                VALUES (?, ?, ?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "transactions")));
        }();

        PreparedStatement insertLedgerTransaction = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (ledger_sequence, hash)
                VALUES (?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_transactions")));
        }();

        PreparedStatement insertSuccessor = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (key, seq, next)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "successor")));
        }();

        PreparedStatement insertDiff = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (seq, key)
                VALUES (?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "diff")));
        }();

        PreparedStatement insertAccountTx = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (account, seq_idx, hash)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "account_tx")));
        }();

        PreparedStatement insertNFT = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (token_id, sequence, owner, is_burned)
                VALUES (?, ?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_tokens")));
        }();

        PreparedStatement insertIssuerNFT = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (issuer, taxon, token_id)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")));
        }();

        PreparedStatement insertNFTURI = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (token_id, sequence, uri)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_uris")));
        }();

        PreparedStatement insertNFTTx = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (token_id, seq_idx, hash)
                VALUES (?, ?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")));
        }();

        PreparedStatement insertLedgerHeader = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (sequence, header)
                VALUES (?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "ledgers")));
        }();

        PreparedStatement insertLedgerHash = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                INSERT INTO {} 
                       (hash, sequence)
                VALUES (?, ?)
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_hashes")));
        }();

        //
        // Update (and "delete") queries
        //

        PreparedStatement updateLedgerRange = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                UPDATE {} 
                   SET sequence = ?
                 WHERE is_latest = ? 
                    IF sequence IN (?, null)
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_range")));
        }();

        PreparedStatement deleteLedgerRange = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                UPDATE {} 
                   SET sequence = ?
                 WHERE is_latest = false
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_range")));
        }();

        //
        // Select queries
        //

        PreparedStatement selectSuccessor = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT next 
                  FROM {}               
                 WHERE key = ?
                   AND seq <= ?
              ORDER BY seq DESC 
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "successor")));
        }();

        PreparedStatement selectDiff = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT key 
                  FROM {}
                 WHERE seq = ?
                )",
                qualifiedTableName(settingsProvider_.get(), "diff")));
        }();

        PreparedStatement selectObject = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT object, sequence 
                  FROM {}               
                 WHERE key = ?
                   AND sequence <= ?
              ORDER BY sequence DESC 
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "objects")));
        }();

        PreparedStatement selectTransaction = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT transaction, metadata, ledger_sequence, date 
                  FROM {}
                 WHERE hash = ?
                )",
                qualifiedTableName(settingsProvider_.get(), "transactions")));
        }();

        PreparedStatement selectAllTransactionHashesInLedger = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT hash 
                  FROM {}               
                 WHERE ledger_sequence = ?               
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_transactions")));
        }();

        PreparedStatement selectLedgerPageKeys = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT key 
                  FROM {}               
                 WHERE TOKEN(key) >= ?
                   AND sequence <= ?
         PER PARTITION LIMIT 1 
                 LIMIT ?
                 ALLOW FILTERING
                )",
                qualifiedTableName(settingsProvider_.get(), "objects")));
        }();

        PreparedStatement selectLedgerPage = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT object, key
                  FROM {}
                 WHERE TOKEN(key) >= ?
                   AND sequence <= ?
         PER PARTITION LIMIT 1
                 LIMIT ?
                 ALLOW FILTERING
                )",
                qualifiedTableName(settingsProvider_.get(), "objects")));
        }();

        PreparedStatement getToken = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT TOKEN(key) 
                  FROM {}               
                 WHERE key = ?               
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "objects")));
        }();

        PreparedStatement selectAccountTx = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT hash, seq_idx 
                  FROM {}               
                 WHERE account = ?
                   AND seq_idx <= ?
                 LIMIT ?
                )",
                qualifiedTableName(settingsProvider_.get(), "account_tx")));
        }();

        PreparedStatement selectAccountTxForward = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT hash, seq_idx 
                  FROM {}               
                 WHERE account = ?
                   AND seq_idx >= ?
              ORDER BY seq_idx ASC 
                 LIMIT ?
                )",
                qualifiedTableName(settingsProvider_.get(), "account_tx")));
        }();

        PreparedStatement selectNFT = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT sequence, owner, is_burned
                  FROM {}    
                 WHERE token_id = ?
                   AND sequence <= ?
              ORDER BY sequence DESC
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_tokens")));
        }();

        PreparedStatement selectNFTURI = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT uri
                  FROM {}    
                 WHERE token_id = ?
                   AND sequence <= ?
              ORDER BY sequence DESC
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_uris")));
        }();

        PreparedStatement selectNFTTx = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT hash, seq_idx
                  FROM {}    
                 WHERE token_id = ?
                   AND seq_idx < ?
              ORDER BY seq_idx DESC
                 LIMIT ?
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")));
        }();

        PreparedStatement selectNFTTxForward = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT hash, seq_idx
                  FROM {}    
                 WHERE token_id = ?
                   AND seq_idx >= ?
              ORDER BY seq_idx ASC
                 LIMIT ?
                )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")));
        }();

        PreparedStatement selectNFTIDsByIssuer = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT token_id
                FROM {}    
                WHERE issuer = ?
                  AND taxon >= ?
                  AND token_id > ?
                ORDER BY taxon, token_id ASC
                LIMIT ?
                )",
                qualifiedTableName(
                    settingsProvider_.get(), "issuer_nf_tokens_v2")));
        }();

        PreparedStatement selectNFTIDsByIssuerTaxon = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT token_id
                FROM {}    
                WHERE issuer = ?
                  AND taxon = ?
                  AND token_id > ?
                ORDER BY taxon, token_id ASC
                LIMIT ?
                )",
                qualifiedTableName(
                    settingsProvider_.get(), "issuer_nf_tokens_v2")));
        }();

        PreparedStatement selectLedgerByHash = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT sequence
                  FROM {}
                 WHERE hash = ?     
                 LIMIT 1
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_hashes")));
        }();

        PreparedStatement selectLedgerBySeq = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT header
                  FROM {}
                 WHERE sequence = ?
                )",
                qualifiedTableName(settingsProvider_.get(), "ledgers")));
        }();

        PreparedStatement selectLatestLedger = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT sequence
                  FROM {}    
                 WHERE is_latest = true               
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_range")));
        }();

        PreparedStatement selectLedgerRange = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT sequence
                  FROM {}
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_range")));
        }();
    };

    /**
     * @brief Recreates the prepared statements
     */
    void
    prepareStatements(Handle const& handle)
    {
        log_.info() << "Preparing cassandra statements";
        statements_ = std::make_unique<Statements>(settingsProvider_, handle);
        log_.info() << "Finished preparing statements";
    }

    /**
     * @brief Provides access to statements
     */
    std::unique_ptr<Statements> const&
    operator->() const
    {
        return statements_;
    }

private:
    std::unique_ptr<Statements> statements_{nullptr};
};

}  // namespace Backend::Cassandra
