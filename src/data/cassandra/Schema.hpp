#pragma once

#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/string.hpp>
#include <fmt/compile.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace data::cassandra {

/**
 * @brief Returns the table name qualified with the keyspace and table prefix
 *
 * @tparam SettingsProviderType The settings provider type
 * @param provider The settings provider
 * @param name The name of the table
 * @return The qualified table name
 */
template <SomeSettingsProvider SettingsProviderType>
[[nodiscard]] std::string inline qualifiedTableName(
    SettingsProviderType const& provider,
    std::string_view name
)
{
    return fmt::format(
        "{}.{}{}", provider.getKeyspace(), provider.getTablePrefix().value_or(""), name
    );
}

/**
 * @brief Manages the DB schema and provides access to prepared statements.
 *
 * @tparam SettingsProviderType The settings provider type
 */
template <SomeSettingsProvider SettingsProviderType>
class Schema {
protected:
    util::Logger log_{"Backend"};
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;

public:
    virtual ~Schema() = default;

    /**
     * @brief Shared Schema's between all Schema classes (Cassandra and Keyspace)
     *
     * @param settingsProvider The settings provider
     */
    explicit Schema(SettingsProviderType const& settingsProvider)
        : settingsProvider_{std::cref(settingsProvider)}
    {
    }

    /** @brief CQL statement creating the keyspace (SimpleStrategy) if it does not exist */
    std::string createKeyspace = [this] {
        return fmt::format(
            R"(
            CREATE KEYSPACE IF NOT EXISTS {}
              WITH replication = {{
                     'class': 'SimpleStrategy',
                     'replication_factor': '{}'
                   }}
               AND durable_writes = True
            )",
            settingsProvider_.get().getKeyspace(),
            settingsProvider_.get().getReplicationFactor()
        );
    }();

    // =======================
    // Schema creation queries
    // =======================

    /** @brief CQL statements creating all tables used by Clio if they do not exist */
    std::vector<Statement> createSchema = [this] {
        std::vector<Statement> statements;

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                         key blob,
                    sequence bigint,
                      object blob,
                     PRIMARY KEY (key, sequence)
                  )
             WITH CLUSTERING ORDER BY (sequence DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "objects")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                        hash blob PRIMARY KEY,
             ledger_sequence bigint,
                        date bigint,
                 transaction blob,
                    metadata blob
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "transactions")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
             ledger_sequence bigint,
                        hash blob,
                     PRIMARY KEY (ledger_sequence, hash)
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "ledger_transactions")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    key blob,
                    seq bigint,
                   next blob,
                PRIMARY KEY (key, seq)
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "successor")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    seq bigint,
                    key blob,
                PRIMARY KEY (seq, key)
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "diff")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    account blob,
                    seq_idx tuple<bigint, bigint>,
                       hash blob,
                    PRIMARY KEY (account, seq_idx)
                  )
             WITH CLUSTERING ORDER BY (seq_idx DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "account_tx")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    sequence bigint PRIMARY KEY,
                      header blob
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "ledgers")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    hash blob PRIMARY KEY,
                sequence bigint
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "ledger_hashes")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    is_latest boolean PRIMARY KEY,
                     sequence bigint
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "ledger_range")
            )
        );

        statements.emplace_back(
            fmt::format(
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
            )",
                qualifiedTableName(settingsProvider_.get(), "nf_tokens")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                      issuer blob,
                       taxon bigint,
                    token_id blob,
                     PRIMARY KEY (issuer, taxon, token_id)
                  )
             WITH CLUSTERING ORDER BY (taxon ASC, token_id ASC)
            )",
                qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    token_id blob,
                    sequence bigint,
                         uri blob,
                     PRIMARY KEY (token_id, sequence)
                  )
             WITH CLUSTERING ORDER BY (sequence DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_uris")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                    token_id blob,
                     seq_idx tuple<bigint, bigint>,
                        hash blob,
                     PRIMARY KEY (token_id, seq_idx)
                  )
             WITH CLUSTERING ORDER BY (seq_idx DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
         mptoken_issuance_id blob,
                     seq_idx tuple<bigint, bigint>,
                        hash blob,
                     PRIMARY KEY (mptoken_issuance_id, seq_idx)
                  )
             WITH CLUSTERING ORDER BY (seq_idx DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "mptoken_issuance_transactions")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
         mptoken_issuance_id blob,
                     account blob,
                     seq_idx tuple<bigint, bigint>,
                        hash blob,
                     PRIMARY KEY ((mptoken_issuance_id, account), seq_idx)
                  )
             WITH CLUSTERING ORDER BY (seq_idx DESC)
            )",
                qualifiedTableName(settingsProvider_.get(), "account_mptoken_issuance_transactions")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                      mpt_id blob,
                      holder blob,
                     PRIMARY KEY (mpt_id, holder)
                  )
             WITH CLUSTERING ORDER BY (holder ASC)
            )",
                qualifiedTableName(settingsProvider_.get(), "mp_token_holders")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                   migrator_name TEXT,
                          status TEXT,
                         PRIMARY KEY (migrator_name)
                  )
            )",
                qualifiedTableName(settingsProvider_.get(), "migrator_status")
            )
        );

        statements.emplace_back(
            fmt::format(
                R"(
           CREATE TABLE IF NOT EXISTS {}
                  (
                   node_id UUID,
                   message TEXT,
                   PRIMARY KEY (node_id)
                  )
             WITH default_time_to_live = 2
            )",
                qualifiedTableName(settingsProvider_.get(), "nodes_chat")
            )
        );

        return statements;
    }();

    /**
     * @brief Prepared statements holder.
     */
    class Statements {
    protected:
        std::reference_wrapper<SettingsProviderType const> settingsProvider_;
        std::reference_wrapper<Handle const> handle_;

    public:
        /**
         * @brief Construct a new Statements object
         *
         * @param settingsProvider The settings provider
         * @param handle The handle to the DB
         */
        Statements(SettingsProviderType const& settingsProvider, Handle const& handle)
            : settingsProvider_{settingsProvider}, handle_{std::cref(handle)}
        {
        }

        //
        // Insert queries
        //

        /** @brief Insert a ledger object version into `objects` */
        PreparedStatement insertObject = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (key, sequence, object)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "objects")
                )
            );
        }();

        /** @brief Insert a transaction with its metadata into `transactions` */
        PreparedStatement insertTransaction = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (hash, ledger_sequence, date, transaction, metadata)
                VALUES (?, ?, ?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "transactions")
                )
            );
        }();

        /** @brief Insert a ledger sequence to transaction hash link into `ledger_transactions` */
        PreparedStatement insertLedgerTransaction = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (ledger_sequence, hash)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_transactions")
                )
            );
        }();

        /** @brief Insert a key to successor link at a ledger sequence into `successor` */
        PreparedStatement insertSuccessor = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (key, seq, next)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "successor")
                )
            );
        }();

        /** @brief Insert a key changed in a ledger into `diff` */
        PreparedStatement insertDiff = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (seq, key)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "diff")
                )
            );
        }();

        /** @brief Insert an account to transaction link into `account_tx` */
        PreparedStatement insertAccountTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (account, seq_idx, hash)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "account_tx")
                )
            );
        }();

        /** @brief Insert an NFT state (owner, burned flag) at a ledger sequence into `nf_tokens` */
        PreparedStatement insertNFT = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (token_id, sequence, owner, is_burned)
                VALUES (?, ?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_tokens")
                )
            );
        }();

        /** @brief Insert an issuer/taxon to NFT ID link into `issuer_nf_tokens_v2` */
        PreparedStatement insertIssuerNFT = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (issuer, taxon, token_id)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
                )
            );
        }();

        /** @brief Insert an NFT URI at a ledger sequence into `nf_token_uris` */
        PreparedStatement insertNFTURI = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (token_id, sequence, uri)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_token_uris")
                )
            );
        }();

        /** @brief Insert an NFT to transaction link into `nf_token_transactions` */
        PreparedStatement insertNFTTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (token_id, seq_idx, hash)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")
                )
            );
        }();

        /** @brief Insert an MPT issuance to tx link into `mptoken_issuance_transactions` */
        PreparedStatement insertMPTokenIssuanceTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (mptoken_issuance_id, seq_idx, hash)
                VALUES (?, ?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "mptoken_issuance_transactions")
                )
            );
        }();

        /**
         * @brief Insert an MPT issuance/account to transaction link into
         * `account_mptoken_issuance_transactions`
         */
        PreparedStatement insertAccountMPTokenIssuanceTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (mptoken_issuance_id, account, seq_idx, hash)
                VALUES (?, ?, ?, ?)
                )",
                    qualifiedTableName(
                        settingsProvider_.get(), "account_mptoken_issuance_transactions"
                    )
                )
            );
        }();

        /** @brief Insert an MPT ID to holder link into `mp_token_holders` */
        PreparedStatement insertMPTHolder = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (mpt_id, holder)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "mp_token_holders")
                )
            );
        }();

        /** @brief Insert a serialized ledger header by sequence into `ledgers` */
        PreparedStatement insertLedgerHeader = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (sequence, header)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledgers")
                )
            );
        }();

        /** @brief Insert a ledger hash to sequence mapping into `ledger_hashes` */
        PreparedStatement insertLedgerHash = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (hash, sequence)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_hashes")
                )
            );
        }();

        //
        // Update (and "delete") queries
        //

        /** @brief Set the minimum (non-latest) sequence in `ledger_range` */
        PreparedStatement deleteLedgerRange = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                UPDATE {}
                   SET sequence = ?
                 WHERE is_latest = False
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        /** @brief Insert the status of a migrator into `migrator_status` */
        PreparedStatement insertMigratorStatus = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                INSERT INTO {}
                       (migrator_name, status)
                VALUES (?, ?)
                )",
                    qualifiedTableName(settingsProvider_.get(), "migrator_status")
                )
            );
        }();

        /** @brief Update the message of a Clio node in `nodes_chat` */
        PreparedStatement updateClioNodeMessage = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                UPDATE {}
                   SET message = ?
                 WHERE node_id = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "nodes_chat")
                )
            );
        }();

        //
        // Select queries
        //

        /** @brief Select the successor of a key as of a ledger sequence from `successor` */
        PreparedStatement selectSuccessor = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT next
                  FROM {}
                 WHERE key = ?
                   AND seq <= ?
              ORDER BY seq DESC
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "successor")
                )
            );
        }();

        /** @brief Select the keys changed in a ledger from `diff` */
        PreparedStatement selectDiff = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT key
                  FROM {}
                 WHERE seq = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "diff")
                )
            );
        }();

        /** @brief Select the latest version of an object as of a ledger sequence from `objects` */
        PreparedStatement selectObject = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT object, sequence
                  FROM {}
                 WHERE key = ?
                   AND sequence <= ?
              ORDER BY sequence DESC
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "objects")
                )
            );
        }();

        /** @brief Select a transaction with its metadata, sequence and date by hash */
        PreparedStatement selectTransaction = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT transaction, metadata, ledger_sequence, date
                  FROM {}
                 WHERE hash = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "transactions")
                )
            );
        }();

        /** @brief Select all transaction hashes in a ledger */
        PreparedStatement selectAllTransactionHashesInLedger = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash
                  FROM {}
                 WHERE ledger_sequence = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_transactions")
                )
            );
        }();

        /** @brief Select the partition token of an object key from `objects` */
        PreparedStatement getToken = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT TOKEN(key)
                  FROM {}
                 WHERE key = ?
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "objects")
                )
            );
        }();

        /** @brief Select a page of account transactions before a cursor (newest first) */
        PreparedStatement selectAccountTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE account = ?
                   AND seq_idx < ?
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "account_tx")
                )
            );
        }();

        /** @brief Select a page of account transactions after a cursor (oldest first) */
        PreparedStatement selectAccountTxForward = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE account = ?
                   AND seq_idx > ?
              ORDER BY seq_idx ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "account_tx")
                )
            );
        }();

        /** @brief Select the state of an NFT as of a ledger sequence from `nf_tokens` */
        PreparedStatement selectNFT = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT sequence, owner, is_burned
                  FROM {}
                 WHERE token_id = ?
                   AND sequence <= ?
              ORDER BY sequence DESC
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_tokens")
                )
            );
        }();

        /** @brief Select the URI of an NFT as of a ledger sequence from `nf_token_uris` */
        PreparedStatement selectNFTURI = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT uri
                  FROM {}
                 WHERE token_id = ?
                   AND sequence <= ?
              ORDER BY sequence DESC
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_token_uris")
                )
            );
        }();

        /** @brief Select a page of NFT transactions before a cursor (newest first) */
        PreparedStatement selectNFTTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE token_id = ?
                   AND seq_idx < ?
              ORDER BY seq_idx DESC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")
                )
            );
        }();

        /** @brief Select a page of NFT transactions from a cursor (oldest first) */
        PreparedStatement selectNFTTxForward = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE token_id = ?
                   AND seq_idx >= ?
              ORDER BY seq_idx ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "nf_token_transactions")
                )
            );
        }();

        /** @brief Select a page of MPT issuance transactions before a cursor (newest first) */
        PreparedStatement selectMPTokenIssuanceTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE mptoken_issuance_id = ?
                   AND seq_idx < ?
              ORDER BY seq_idx DESC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "mptoken_issuance_transactions")
                )
            );
        }();

        /** @brief Select a page of MPT issuance transactions from a cursor (oldest first) */
        PreparedStatement selectMPTokenIssuanceTxForward = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE mptoken_issuance_id = ?
                   AND seq_idx >= ?
              ORDER BY seq_idx ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "mptoken_issuance_transactions")
                )
            );
        }();

        /** @brief Select a page of an account's MPT issuance txs before a cursor (newest first) */
        PreparedStatement selectAccountMPTokenIssuanceTx = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE mptoken_issuance_id = ?
                   AND account = ?
                   AND seq_idx < ?
              ORDER BY seq_idx DESC
                 LIMIT ?
                )",
                    qualifiedTableName(
                        settingsProvider_.get(), "account_mptoken_issuance_transactions"
                    )
                )
            );
        }();

        /** @brief Select a page of an account's MPT issuance txs from a cursor (oldest first) */
        PreparedStatement selectAccountMPTokenIssuanceTxForward = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT hash, seq_idx
                  FROM {}
                 WHERE mptoken_issuance_id = ?
                   AND account = ?
                   AND seq_idx >= ?
              ORDER BY seq_idx ASC
                 LIMIT ?
                )",
                    qualifiedTableName(
                        settingsProvider_.get(), "account_mptoken_issuance_transactions"
                    )
                )
            );
        }();

        /** @brief Select a page of NFT IDs for an issuer and taxon after a token ID */
        PreparedStatement selectNFTIDsByIssuerTaxon = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT token_id
                  FROM {}
                 WHERE issuer = ?
                   AND taxon = ?
                   AND token_id > ?
              ORDER BY taxon ASC, token_id ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
                )
            );
        }();

        /** @brief Select a page of holders of an MPT after a given holder */
        PreparedStatement selectMPTHolders = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT holder
                  FROM {}
                 WHERE mpt_id = ?
                   AND holder > ?
              ORDER BY holder ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "mp_token_holders")
                )
            );
        }();

        /** @brief Select the ledger sequence for a ledger hash from `ledger_hashes` */
        PreparedStatement selectLedgerByHash = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT sequence
                  FROM {}
                 WHERE hash = ?
                 LIMIT 1
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_hashes")
                )
            );
        }();

        /** @brief Select the serialized ledger header by sequence from `ledgers` */
        PreparedStatement selectLedgerBySeq = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT header
                  FROM {}
                 WHERE sequence = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledgers")
                )
            );
        }();

        /** @brief Select the latest ledger sequence from `ledger_range` */
        PreparedStatement selectLatestLedger = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT sequence
                  FROM {}
                 WHERE is_latest = True
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        /** @brief Select both the min and max sequences from `ledger_range` */
        PreparedStatement selectLedgerRange = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT sequence
                  FROM {}
                 WHERE is_latest in (True, False)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        /** @brief Select the status of a migrator by name from `migrator_status` */
        PreparedStatement selectMigratorStatus = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT status
                  FROM {}
                 WHERE migrator_name = ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "migrator_status")
                )
            );
        }();

        /** @brief Select the IDs and messages of all Clio nodes from `nodes_chat` */
        PreparedStatement selectClioNodesData = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT node_id, message
                  FROM {}
                )",
                    qualifiedTableName(settingsProvider_.get(), "nodes_chat")
                )
            );
        }();
    };

    /**
     * @brief Recreates the prepared statements.
     *
     * @param handle The handle to the DB
     */
    virtual void
    prepareStatements(Handle const& handle) = 0;
};

}  // namespace data::cassandra
