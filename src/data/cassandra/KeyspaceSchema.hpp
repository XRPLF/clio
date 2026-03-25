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
 */
template <SomeSettingsProvider SettingsProviderType>
class KeyspaceSchema : public Schema<SettingsProvider> {
public:
    using Schema::Schema;

    /**
     * @brief Construct a new Keyspace Schema object
     *
     * @param settingsProvider The settings provider
     */
    struct KeyspaceStatements : public Schema<SettingsProvider>::Statements {
        using Schema<SettingsProvider>::Statements::Statements;

        //
        // Insert queries
        //
        PreparedStatement insertLedgerRange = [this]() {
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
        PreparedStatement updateLedgerRange = [this]() {
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

        PreparedStatement selectLedgerRange = [this]() {
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
        PreparedStatement selectNFTsAfterTaxonKeyspaces = [this]() {
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
