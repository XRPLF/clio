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
#include "data/cassandra/Types.hpp"

#include <string>
#include <vector>

namespace data::cassandra {

/**
 * @brief An abstract interface for managing the DB schema and providing access
 * to prepared statements.
 */
template <SomeSettingsProvider SettingsProviderType>
class AbstractSchema {
public:
    virtual ~AbstractSchema() = default;

    // Create
    virtual std::string const&
    getCreateKeyspaceQuery() const = 0;
    virtual std::vector<Statement> const&
    getCreateSchemaQueries() const = 0;
    virtual void
    prepareStatements(Handle const& handle) = 0;

    // Insert
    virtual PreparedStatement const&
    insertObject() const = 0;
    virtual PreparedStatement const&
    insertTransaction() const = 0;
    virtual PreparedStatement const&
    insertLedgerTransaction() const = 0;
    virtual PreparedStatement const&
    insertSuccessor() const = 0;
    virtual PreparedStatement const&
    insertDiff() const = 0;
    virtual PreparedStatement const&
    insertAccountTx() const = 0;
    virtual PreparedStatement const&
    insertNFT() const = 0;
    virtual PreparedStatement const&
    insertIssuerNFT() const = 0;
    virtual PreparedStatement const&
    insertNFTURI() const = 0;
    virtual PreparedStatement const&
    insertNFTTx() const = 0;
    virtual PreparedStatement const&
    insertMPTHolder() const = 0;
    virtual PreparedStatement const&
    insertLedgerHeader() const = 0;
    virtual PreparedStatement const&
    insertLedgerHash() const = 0;
    virtual PreparedStatement const&
    insertMigratorStatus() const = 0;

    // Update
    virtual PreparedStatement const&
    updateLedgerRange() const = 0;
    virtual PreparedStatement const&
    deleteLedgerRange() const = 0;
    virtual PreparedStatement const&
    updateClioNodeMessage() const = 0;

    // Select
    virtual PreparedStatement const&
    selectSuccessor() const = 0;
    virtual PreparedStatement const&
    selectDiff() const = 0;
    virtual PreparedStatement const&
    selectObject() const = 0;
    virtual PreparedStatement const&
    selectTransaction() const = 0;
    virtual PreparedStatement const&
    selectAllTransactionHashesInLedger() const = 0;
    virtual PreparedStatement const&
    setToken() const = 0;
    virtual PreparedStatement const&
    selectAccountTx() const = 0;
    virtual PreparedStatement const&
    selectAccountTxForward() const = 0;
    virtual PreparedStatement const&
    selectNFT() const = 0;
    virtual PreparedStatement const&
    selectNFTURI() const = 0;
    virtual PreparedStatement const&
    selectNFTTx() const = 0;
    virtual PreparedStatement const&
    selectNFTTxForward() const = 0;
    virtual PreparedStatement const&
    selectNFTIDsByIssuerTaxon() const = 0;
    virtual PreparedStatement const&
    selectMPTHolders() const = 0;
    virtual PreparedStatement const&
    selectLedgerByHash() const = 0;
    virtual PreparedStatement const&
    selectLedgerBySeq() const = 0;
    virtual PreparedStatement const&
    selectLatestLedger() const = 0;
    virtual PreparedStatement const&
    selectLedgerRange() const = 0;
    virtual PreparedStatement const&
    selectMigratorStatus() const = 0;
    virtual PreparedStatement const&
    selectClioNodesData() const = 0;
};

}  // namespace data::cassandra
