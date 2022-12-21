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

namespace clio::data {

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
    std::uint32_t ledgerSequence;
    std::uint32_t date;
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
    bool isBurned;

    // clearly two tokens are the same if they have the same ID, but this
    // struct stores the state of a given token at a given ledger sequence, so
    // we also need to compare with ledgerSequence
    bool
    operator==(NFT const& other) const
    {
        return tokenID == other.tokenID &&
            ledgerSequence == other.ledgerSequence;
    }
};

struct LedgerRange
{
    std::uint32_t minSequence;
    std::uint32_t maxSequence;
};

constexpr ripple::uint256 firstKey{
    "0000000000000000000000000000000000000000000000000000000000000000"};
constexpr ripple::uint256 lastKey{
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"};
constexpr ripple::uint256 hi192{
    "0000000000000000000000000000000000000000000000001111111111111111"};

}  // namespace clio::data
