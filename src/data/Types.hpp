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

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace data {

using Blob = std::vector<unsigned char>;

/**
 * @brief Represents an object in the ledger.
 */
struct LedgerObject {
    ripple::uint256 key;
    Blob blob;

    bool
    operator==(LedgerObject const& other) const = default;
};

/**
 * @brief Represents a page of LedgerObjects.
 */
struct LedgerPage {
    std::vector<LedgerObject> objects;
    std::optional<ripple::uint256> cursor;
};

/**
 * @brief Represents a page of book offer objects.
 */
struct BookOffersPage {
    std::vector<LedgerObject> offers;
    std::optional<ripple::uint256> cursor;
};

/**
 * @brief Represents a transaction and its metadata bundled together.
 */
struct TransactionAndMetadata {
    Blob transaction;
    Blob metadata;
    std::uint32_t ledgerSequence = 0;
    std::uint32_t date = 0;

    TransactionAndMetadata() = default;

    /**
     * @brief Construct a new Transaction And Metadata object
     *
     * @param transaction The transaction
     * @param metadata The metadata
     * @param ledgerSequence The ledger sequence
     * @param date The date
     */
    TransactionAndMetadata(Blob transaction, Blob metadata, std::uint32_t ledgerSequence, std::uint32_t date)
        : transaction{std::move(transaction)}, metadata{std::move(metadata)}, ledgerSequence{ledgerSequence}, date{date}
    {
    }

    /**
     * @brief Construct a new Transaction And Metadata object
     *
     * @param data The data to construct from
     */
    TransactionAndMetadata(std::tuple<Blob, Blob, std::uint32_t, std::uint32_t> data)
        : transaction{std::get<0>(data)}
        , metadata{std::get<1>(data)}
        , ledgerSequence{std::get<2>(data)}
        , date{std::get<3>(data)}
    {
    }

    /**
     * @brief Check if the transaction and metadata are the same as another
     *
     * @param other The other transaction and metadata
     * @return true if they are the same; false otherwise
     */
    bool
    operator==(TransactionAndMetadata const& other) const
    {
        return transaction == other.transaction && metadata == other.metadata &&
            ledgerSequence == other.ledgerSequence && date == other.date;
    }
};

/**
 * @brief Represents a cursor into the transactions table.
 */
struct TransactionsCursor {
    std::uint32_t ledgerSequence = 0;
    std::uint32_t transactionIndex = 0;

    TransactionsCursor() = default;

    /**
     * @brief Construct a new Transactions Cursor object
     *
     * @param ledgerSequence The ledger sequence
     * @param transactionIndex The transaction index
     */
    TransactionsCursor(std::uint32_t ledgerSequence, std::uint32_t transactionIndex)
        : ledgerSequence{ledgerSequence}, transactionIndex{transactionIndex}
    {
    }

    /**
     * @brief Construct a new Transactions Cursor object
     *
     * @param data The data to construct from
     */
    TransactionsCursor(std::tuple<std::uint32_t, std::uint32_t> data)
        : ledgerSequence{std::get<0>(data)}, transactionIndex{std::get<1>(data)}
    {
    }

    bool
    operator==(TransactionsCursor const& other) const = default;

    /**
     * @brief Convert the cursor to a tuple of seq and index
     *
     * @return The cursor as a tuple
     */
    [[nodiscard]] std::tuple<std::uint32_t, std::uint32_t>
    asTuple() const
    {
        return std::make_tuple(ledgerSequence, transactionIndex);
    }
};

/**
 * @brief Represests a bundle of transactions with metadata and a cursor to the next page.
 */
struct TransactionsAndCursor {
    std::vector<TransactionAndMetadata> txns;
    std::optional<TransactionsCursor> cursor;
};

/**
 * @brief Represents a NFToken.
 */
struct NFT {
    ripple::uint256 tokenID;
    std::uint32_t ledgerSequence{};
    ripple::AccountID owner;
    Blob uri;
    bool isBurned{};

    NFT() = default;

    /**
     * @brief Construct a new NFT object
     *
     * @param tokenID The token ID
     * @param ledgerSequence The ledger sequence
     * @param owner The owner
     * @param uri The URI
     * @param isBurned Whether the token is burned
     */
    NFT(ripple::uint256 const& tokenID,
        std::uint32_t ledgerSequence,
        ripple::AccountID const& owner,
        Blob uri,
        bool isBurned)
        : tokenID{tokenID}, ledgerSequence{ledgerSequence}, owner{owner}, uri{std::move(uri)}, isBurned{isBurned}
    {
    }

    /**
     * @brief Construct a new NFT object
     *
     * @param tokenID The token ID
     * @param ledgerSequence The ledger sequence
     * @param owner The owner
     * @param isBurned Whether the token is burned
     */
    NFT(ripple::uint256 const& tokenID, std::uint32_t ledgerSequence, ripple::AccountID const& owner, bool isBurned)
        : NFT(tokenID, ledgerSequence, owner, {}, isBurned)
    {
    }

    /**
     * @brief Check if the NFT is the same as another
     *
     * Clearly two tokens are the same if they have the same ID, but this struct stores the state of a given
     * token at a given ledger sequence, so we also need to compare with ledgerSequence.
     *
     * @param other The other NFT
     * @return true if they are the same; false otherwise
     */
    bool
    operator==(NFT const& other) const
    {
        return tokenID == other.tokenID && ledgerSequence == other.ledgerSequence;
    }
};

/**
 * @brief Represents a bundle of NFTs with a cursor to the next page
 */
struct NFTsAndCursor {
    std::vector<NFT> nfts;
    std::optional<ripple::uint256> cursor;
};

/**
 * @brief Stores a range of sequences as a min and max pair.
 */
struct LedgerRange {
    std::uint32_t minSequence = 0;
    std::uint32_t maxSequence = 0;
};

/**
 * @brief Represents an amendment in the XRPL
 */
struct Amendment {
    std::string name;
    ripple::uint256 feature;
    bool isSupportedByXRPL;
    bool isSupportedByClio;
    bool isRetired;

    /**
     * @brief Get the amendment Id from its name
     *
     * @param name The name of the amendment
     * @return The amendment Id as uint256
     */
    static ripple::uint256
    GetAmendmentId(std::string_view const name);
};

/**
 * @brief A helper for amendment name to feature conversions
 */
struct AmendmentKey {
    std::string name;

    AmendmentKey(std::convertible_to<std::string> auto&& val) : name{std::forward<decltype(val)>(val)}
    {
    }

    /** @brief Conversion to string */
    operator std::string const&() const;

    /** @brief Conversion to uint256 */
    operator ripple::uint256() const;

    /**
     * @brief Comparison operators
     * @param other The object to compare to
     * @return Whether the objects are equal, greater or less
     */
    auto
    operator<=>(AmendmentKey const& other) const = default;
};

constexpr ripple::uint256 firstKey{"0000000000000000000000000000000000000000000000000000000000000000"};
constexpr ripple::uint256 lastKey{"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"};
constexpr ripple::uint256 hi192{"0000000000000000000000000000000000000000000000001111111111111111"};

}  // namespace data
