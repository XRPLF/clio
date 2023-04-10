//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>
#include <optional>
#include <string>
#include <vector>

namespace Backend {

// *** return types

using Blob = std::vector<unsigned char>;

struct LedgerObject
{
    ripple::uint256 key;
    Blob blob;
    bool
    operator==(const LedgerObject& other) const
    {
        return key == other.key && blob == other.blob;
    }
};

struct LedgerPage
{
    std::vector<LedgerObject> objects;
    std::optional<ripple::uint256> cursor;
};
struct BookOffersPage
{
    std::vector<LedgerObject> offers;
    std::optional<ripple::uint256> cursor;
};
struct TransactionAndMetadata
{
    Blob transaction;
    Blob metadata;
    std::uint32_t ledgerSequence = 0;
    std::uint32_t date = 0;

    TransactionAndMetadata() = default;
    TransactionAndMetadata(
        Blob const& transaction,
        Blob const& metadata,
        std::uint32_t ledgerSequence,
        std::uint32_t date)
        : transaction{transaction}, metadata{metadata}, ledgerSequence{ledgerSequence}, date{date}
    {
    }

    TransactionAndMetadata(std::tuple<Blob, Blob, std::uint32_t, std::uint32_t> data)
        : transaction{std::get<0>(data)}
        , metadata{std::get<1>(data)}
        , ledgerSequence{std::get<2>(data)}
        , date{std::get<3>(data)}
    {
    }

    bool
    operator==(const TransactionAndMetadata& other) const
    {
        return transaction == other.transaction && metadata == other.metadata &&
            ledgerSequence == other.ledgerSequence && date == other.date;
    }
};

struct TransactionsCursor
{
    std::uint32_t ledgerSequence;
    std::uint32_t transactionIndex;

    TransactionsCursor() = default;
    TransactionsCursor(std::uint32_t ledgerSequence, std::uint32_t transactionIndex)
        : ledgerSequence{ledgerSequence}, transactionIndex{transactionIndex}
    {
    }

    TransactionsCursor(std::tuple<std::uint32_t, std::uint32_t> data)
        : ledgerSequence{std::get<0>(data)}, transactionIndex{std::get<1>(data)}
    {
    }

    TransactionsCursor&
    operator=(TransactionsCursor const&) = default;

    bool
    operator==(TransactionsCursor const& other) const = default;

    [[nodiscard]] std::tuple<std::uint32_t, std::uint32_t>
    asTuple() const
    {
        return std::make_tuple(ledgerSequence, transactionIndex);
    }
};

struct TransactionsAndCursor
{
    std::vector<TransactionAndMetadata> txns;
    std::optional<TransactionsCursor> cursor;
};

struct NFT
{
    ripple::uint256 tokenID;
    std::uint32_t ledgerSequence;
    ripple::AccountID owner;
    Blob uri;
    bool isBurned;

    NFT() = default;
    NFT(ripple::uint256 const& tokenID,
        std::uint32_t ledgerSequence,
        ripple::AccountID const& owner,
        Blob const& uri,
        bool isBurned)
        : tokenID{tokenID}, ledgerSequence{ledgerSequence}, owner{owner}, uri{uri}, isBurned{isBurned}
    {
    }

    NFT(ripple::uint256 const& tokenID, std::uint32_t ledgerSequence, ripple::AccountID const& owner, bool isBurned)
        : NFT(tokenID, ledgerSequence, owner, {}, isBurned)
    {
    }

    // clearly two tokens are the same if they have the same ID, but this
    // struct stores the state of a given token at a given ledger sequence, so
    // we also need to compare with ledgerSequence
    bool
    operator==(NFT const& other) const
    {
        return tokenID == other.tokenID && ledgerSequence == other.ledgerSequence;
    }
};

struct NFTsAndCursor
{
    std::vector<NFT> nfts;
    std::optional<ripple::uint256> cursor;
};

struct LedgerRange
{
    std::uint32_t minSequence;
    std::uint32_t maxSequence;
};
constexpr ripple::uint256 firstKey{"0000000000000000000000000000000000000000000000000000000000000000"};
constexpr ripple::uint256 lastKey{"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"};
constexpr ripple::uint256 hi192{"0000000000000000000000000000000000000000000000001111111111111111"};
}  // namespace Backend
