//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
[[nodiscard]] std::string inline tableName(SettingsProviderType const& provider, std::string_view name)
{
    return fmt::format("{}.{}{}", provider.getKeyspace(), provider.getTablePrefix().value_or(""), name);
}

/**
 * @brief Manages the DB schema and provides access to prepared statements.
 */
template <SomeSettingsProvider SettingsProviderType>
class KeyspaceSchema : public Schema<SettingsProvider> {
public:
    using Schema::Schema;

    struct KeyspaceStatements : public Schema<SettingsProvider>::Statements {
        using Schema<SettingsProvider>::Statements::Statements;

        //
        // Insert queries
        //
        PreparedStatement insertLedgerRange = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                    INSERT INTO {} (is_latest, sequence) VALUES (?, ?) IF NOT EXISTS
                    )",
                tableName(settingsProvider_.get(), "ledger_range")
            ));
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
                    IF sequence = ?
                )",
                tableName(settingsProvider_.get(), "ledger_range")
            ));
        }();

        //
        // Select queries
        //
        PreparedStatement selectNFTIDsByIssuerTaxon = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                SELECT token_id
                  FROM {}
                 WHERE issuer = ?
                   AND taxon = ?
                   AND token_id > ?
              ORDER BY taxon ASC, token_id ASC
                 LIMIT ?
                )",
                tableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
            ));
        }();

        PreparedStatement selectNFTsAfterTaxonKeyspaces = [this]() {
            return handle_.get().prepare(fmt::format(
                R"(
                        SELECT token_id
                          FROM {}
                         WHERE issuer = ?
                           AND taxon > ?
                      ORDER BY taxon ASC, token_id ASC
                         LIMIT ?
                )",
                tableName(settingsProvider_.get(), "issuer_nf_tokens_v2")
            ));
        }();
    };

    void
    prepareStatements(Handle const& handle) override
    {
        LOG(log_.info()) << "Preparing keyspace statements";
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
