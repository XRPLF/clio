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
class CassandraSchema : public Schema<SettingsProvider> {
    using Schema::Schema;

public:
    /**
     * @brief Construct a new Cassandra Schema object
     *
     * @param settingsProvider The settings provider
     */
    struct CassandraStatements : public Schema<SettingsProvider>::Statements {
        using Schema<SettingsProvider>::Statements::Statements;

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
                    IF sequence IN (?, null)
                )",
                    qualifiedTableName(settingsProvider_.get(), "ledger_range")
                )
            );
        }();

        //
        // Select queries
        //

        PreparedStatement selectNFTIDsByIssuer = [this]() {
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

        PreparedStatement selectAccountFromBeginning = [this]() {
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

        PreparedStatement selectAccountFromToken = [this]() {
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

        PreparedStatement selectLedgerPageKeys = [this]() {
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

        PreparedStatement selectLedgerPage = [this]() {
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
