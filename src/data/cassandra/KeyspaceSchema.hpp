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
class KeyspaceSchema : public Schema<SettingsProvider> {
public:
    using Schema::Schema;

    /**
     * @brief Construct a new Keyspace Schema object
     */
    struct KeyspaceStatements : public Schema<SettingsProvider>::Statements {
        using Schema<SettingsProvider>::Statements::Statements;

        //
        // Insert queries
        //
        /** @brief Insert a `ledger_range` row (min or max sequence) if it does not exist */
        PreparedStatement insertLedgerRange = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                    INSERT INTO {} (is_latest, sequence) VALUES (?, ?) IF NOT EXISTS
                    )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        //
        // Update (and "delete") queries
        //
        /** @brief Conditionally update a `ledger_range` sequence (LWT on the previous value) */
        PreparedStatement updateLedgerRange = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                UPDATE {}
                   SET sequence = ?
                 WHERE is_latest = ?
                    IF sequence = ?
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

        //
        // Select queries
        //
        /** @brief Select a page of NFT IDs for an issuer with taxon above a given one */
        PreparedStatement selectNFTsAfterTaxonKeyspaces = [this] {
            return handle_.get().prepare(
                fmt::format(
                    R"(
                        SELECT token_id
                          FROM {}
                         WHERE issuer = ?
                           AND taxon > ?
                      ORDER BY taxon ASC, token_id ASC
                         LIMIT ?
                )",
                    qualifiedTableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
                )
            );
        }();
    };

    /**
     * @brief Recreates the prepared statements, including the Keyspace-specific ones.
     *
     * @param handle The handle to the DB
     */
    void
    prepareStatements(Handle const& handle) override
    {
        LOG(log_.info()) << "Preparing aws keyspace statements";
        statements_ = std::make_unique<KeyspaceStatements>(settingsProvider_, handle);
        LOG(log_.info()) << "Finished preparing statements";
    }

    /**
     * @brief Provides access to statements.
     *
     * @return The statements
     */
    std::unique_ptr<KeyspaceStatements> const&
    operator->() const
    {
        return statements_;
    }

private:
    std::unique_ptr<KeyspaceStatements> statements_{nullptr};
};

}  // namespace data::cassandra
