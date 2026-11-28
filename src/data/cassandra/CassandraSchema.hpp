#pragma once

#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/string.hpp>
#include <fmt/compile.h>

#include <functional>
#include <memory>

namespace data::cassandra {

/**
 * @brief Manages the DB schema and provides access to prepared statements.
 *
 * @tparam SettingsProviderType Unused; the base Schema always uses SettingsProvider
 */
template <SomeSettingsProvider SettingsProviderType>
class CassandraSchema : public Schema<SettingsProvider> {
    using Schema::Schema;

public:
    /**
     * @brief Construct a new Cassandra Schema object
     */
    struct CassandraStatements : public Schema<SettingsProvider>::Statements {
        using Schema<SettingsProvider>::Statements::Statements;

        //
        // Update (and "delete") queries
        //
        /** @brief Conditionally update a `ledger_range` sequence (LWT on previous value or null) */
        PreparedStatement updateLedgerRange = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                UPDATE {}
                   SET sequence = ?
                 WHERE is_latest = ?
                    IF sequence IN (?, null)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        //
        // Select queries
        //

        /** @brief Select a page of NFT IDs for an issuer after a (taxon, token ID) cursor */
        PreparedStatement selectNFTIDsByIssuer = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT token_id
                  FROM {}
                 WHERE issuer = ?
                   AND (taxon, token_id) > ?
              ORDER BY taxon ASC, token_id ASC
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
                )
            );
        }();

        /** @brief Select a page of distinct accounts from the start of `account_tx` */
        PreparedStatement selectAccountFromBeginning = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT account
                  FROM {}
                 WHERE token(account) > 0
                   PER PARTITION LIMIT 1
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "account_tx")
                )
            );
        }();

        /** @brief Select a page of distinct accounts in `account_tx` after a given account */
        PreparedStatement selectAccountFromToken = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT account
                  FROM {}
                 WHERE token(account) > token(?)
                   PER PARTITION LIMIT 1
                 LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "account_tx")
                )
            );
        }();

        /** @brief Select a page of object keys from a token as of a ledger sequence */
        PreparedStatement selectLedgerPageKeys = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT key
                  FROM {}
                 WHERE TOKEN(key) >= ?
                   AND sequence <= ?
         PER PARTITION LIMIT 1
                 LIMIT ?
                 ALLOW FILTERING
                )",
                    qualifiedTableName(settingsProvider_.get(), "objects")
                )
            );
        }();

        /** @brief Select a page of objects and keys from a token as of a ledger sequence */
        PreparedStatement selectLedgerPage = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                SELECT object, key
                  FROM {}
                 WHERE TOKEN(key) >= ?
                   AND sequence <= ?
         PER PARTITION LIMIT 1
                 LIMIT ?
                 ALLOW FILTERING
                )",
                    qualifiedTableName(settingsProvider_.get(), "objects")
                )
            );
        }();
    };

    /**
     * @brief Recreates the prepared statements, including the Cassandra-specific ones.
     *
     * @param handle The handle to the DB
     */
    void
    prepareStatements(Handle const& handle) override
    {
        LOG(log_.info()) << "Preparing cassandra statements";
        statements_ = std::make_unique<CassandraStatements>(settingsProvider_, handle);
        LOG(log_.info()) << "Finished preparing statements";
    }

    /**
     * @brief Provides access to statements.
     *
     * @return The statements
     */
    std::unique_ptr<CassandraStatements> const&
    operator->() const
    {
        return statements_;
    }

private:
    std::unique_ptr<CassandraStatements> statements_{nullptr};
};

}  // namespace data::cassandra
