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

/** @file */
#pragma once

#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/TxMeta.h>

#include <boost/container/flat_set.hpp>

#include <data/Types.h>

/**
 * @brief Struct used to keep track of what to write to account_transactions/account_tx tables.
 */
struct AccountTransactionsData {
    boost::container::flat_set<ripple::AccountID> accounts;
    std::uint32_t ledgerSequence{};
    std::uint32_t transactionIndex{};
    ripple::uint256 txHash;

    AccountTransactionsData(ripple::TxMeta& meta, ripple::uint256 const& txHash)
        : accounts(meta.getAffectedAccounts())
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , txHash(txHash)
    {
    }

    AccountTransactionsData() = default;
};

/**
 * @brief Represents a link from a tx to an NFT that was targeted/modified/created by it.
 *
 * Gets written to nf_token_transactions table and the like.
 */
struct NFTTransactionsData {
    ripple::uint256 tokenID;
    std::uint32_t ledgerSequence;
    std::uint32_t transactionIndex;
    ripple::uint256 txHash;

    NFTTransactionsData(ripple::uint256 const& tokenID, ripple::TxMeta const& meta, ripple::uint256 const& txHash)
        : tokenID(tokenID), ledgerSequence(meta.getLgrSeq()), transactionIndex(meta.getIndex()), txHash(txHash)
    {
    }
};

/**
 * @brief Represents an NFT state at a particular ledger.
 *
 * Gets written to nf_tokens table and the like.
 */
struct NFTsData {
    ripple::uint256 tokenID;
    std::uint32_t ledgerSequence;

    // The transaction index is only stored because we want to store only the
    // final state of an NFT per ledger. Since we pull this from transactions
    // we keep track of which tx index created this so we can de-duplicate, as
    // it is possible for one ledger to have multiple txs that change the
    // state of the same NFT. This field is not applicable when we are loading
    // initial NFT state via ledger objects, since we do not have to tiebreak
    // NFT state for a given ledger in that case.
    std::optional<std::uint32_t> transactionIndex;
    ripple::AccountID owner;
    // We only set the uri if this is a mint tx, or if we are
    // loading initial state from NFTokenPage objects. In other words,
    // uri should only be set if the etl process believes this NFT hasn't
    // been seen before in our local database. We do this so that we don't
    // write to the the nf_token_uris table every
    // time the same NFT changes hands. We also can infer if there is a URI
    // that we need to write to the issuer_nf_tokens table.
    std::optional<ripple::Blob> uri;
    bool isBurned = false;

    // This constructor is used when parsing an NFTokenMint tx.
    // Unfortunately because of the extreme edge case of being able to
    // re-mint an NFT with the same ID, we must explicitly record a null
    // URI. For this reason, we _always_ write this field as a result of
    // this tx.
    NFTsData(
        ripple::uint256 const& tokenID,
        ripple::AccountID const& owner,
        ripple::Blob const& uri,
        ripple::TxMeta const& meta
    )
        : tokenID(tokenID), ledgerSequence(meta.getLgrSeq()), transactionIndex(meta.getIndex()), owner(owner), uri(uri)
    {
    }

    // This constructor is used when parsing an NFTokenBurn or
    // NFTokenAcceptOffer tx
    NFTsData(ripple::uint256 const& tokenID, ripple::AccountID const& owner, ripple::TxMeta const& meta, bool isBurned)
        : tokenID(tokenID)
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , owner(owner)
        , isBurned(isBurned)
    {
    }

    // This constructor is used when parsing an NFTokenPage directly from
    // ledger state.
    // Unfortunately because of the extreme edge case of being able to
    // re-mint an NFT with the same ID, we must explicitly record a null
    // URI. For this reason, we _always_ write this field as a result of
    // this tx.
    NFTsData(
        ripple::uint256 const& tokenID,
        std::uint32_t const ledgerSequence,
        ripple::AccountID const& owner,
        ripple::Blob const& uri
    )
        : tokenID(tokenID), ledgerSequence(ledgerSequence), owner(owner), uri(uri)
    {
    }
};

/**
 * @brief Check whether the supplied object is an offer.
 *
 * @param object The object to check
 * @return true if the object is an offer; false otherwise
 */
template <class T>
inline bool
isOffer(T const& object)
{
    static constexpr short OFFER_OFFSET = 0x006f;
    static constexpr short SHIFT = 8;

    short offer_bytes = (object[1] << SHIFT) | object[2];
    return offer_bytes == OFFER_OFFSET;
}

/**
 * @brief Check whether the supplied hex represents an offer object.
 *
 * @param object The object to check
 * @return true if the object is an offer; false otherwise
 */
template <class T>
inline bool
isOfferHex(T const& object)
{
    auto blob = ripple::strUnHex(4, object.begin(), object.begin() + 4);
    if (blob)
        return isOffer(*blob);
    return false;
}

/**
 * @brief Check whether the supplied object is a dir node.
 *
 * @param object The object to check
 * @return true if the object is a dir node; false otherwise
 */
template <class T>
inline bool
isDirNode(T const& object)
{
    static constexpr short DIR_NODE_SPACE_KEY = 0x0064;
    short const spaceKey = (object.data()[1] << 8) | object.data()[2];
    return spaceKey == DIR_NODE_SPACE_KEY;
}

/**
 * @brief Check whether the supplied object is a book dir.
 *
 * @param key The key into the object
 * @param object The object to check
 * @return true if the object is a book dir; false otherwise
 */
template <class T, class R>
inline bool
isBookDir(T const& key, R const& object)
{
    if (!isDirNode(object))
        return false;

    ripple::STLedgerEntry const sle{ripple::SerialIter{object.data(), object.size()}, key};
    return !sle[~ripple::sfOwner].has_value();
}

/**
 * @brief Get the book out of an offer object.
 *
 * @param offer The offer to get the book for
 * @return Book as ripple::uint256
 */
template <class T>
inline ripple::uint256
getBook(T const& offer)
{
    ripple::SerialIter it{offer.data(), offer.size()};
    ripple::SLE const sle{it, {}};
    ripple::uint256 book = sle.getFieldH256(ripple::sfBookDirectory);

    return book;
}

/**
 * @brief Get the book base.
 *
 * @param key The key to get the book base out of
 * @return Book base as ripple::uint256
 */
template <class T>
inline ripple::uint256
getBookBase(T const& key)
{
    static constexpr size_t KEY_SIZE = 24;

    assert(key.size() == ripple::uint256::size());

    ripple::uint256 ret;
    for (size_t i = 0; i < KEY_SIZE; ++i)
        ret.data()[i] = key.data()[i];

    return ret;
}

/**
 * @brief Stringify a ripple::uint256.
 *
 * @param input The input value
 * @return The input value as a string
 */
inline std::string
uint256ToString(ripple::uint256 const& input)
{
    return {reinterpret_cast<char const*>(input.data()), ripple::uint256::size()};
}

/** @brief The ripple epoch start timestamp. Midnight on 1st January 2000. */
static constexpr std::uint32_t rippleEpochStart = 946684800;
