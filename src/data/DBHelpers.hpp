/** @file */
#pragma once

#include "util/Assert.hpp"

#include <boost/container/flat_set.hpp>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Log.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAccount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief Struct used to keep track of what to write to account_transactions/account_tx tables.
 */
struct AccountTransactionsData {
    boost::container::flat_set<ripple::AccountID> accounts;
    std::uint32_t ledgerSequence{};
    std::uint32_t transactionIndex{};
    ripple::uint256 txHash;

    /**
     * @brief Construct a new AccountTransactionsData object
     *
     * @param meta The transaction metadata
     * @param txHash The transaction hash
     */
    AccountTransactionsData(ripple::TxMeta const& meta, ripple::uint256 const& txHash)
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

    /**
     * @brief Construct a new NFTTransactionsData object
     *
     * @param tokenID The token ID
     * @param meta The transaction metadata
     * @param txHash The transaction hash
     */
    NFTTransactionsData(
        ripple::uint256 const& tokenID,
        ripple::TxMeta const& meta,
        ripple::uint256 const& txHash
    )
        : tokenID(tokenID)
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , txHash(txHash)
    {
    }
};

/**
 * @brief Represents an NFT state at a particular ledger.
 *
 * Gets written to nf_tokens table and the like.
 *
 * The transaction index is only stored because we want to store only the final state of an NFT per
 * ledger. Since we pull this from transactions we keep track of which tx index created this so we
 * can de-duplicate, as it is possible for one ledger to have multiple txs that change the state of
 * the same NFT.
 *
 * We only set the uri if this is a mint tx, or if we are loading initial state from NFTokenPage
 * objects.
 */
struct NFTsData {
    ripple::uint256 tokenID;
    std::uint32_t ledgerSequence;
    std::optional<std::uint32_t> transactionIndex;
    ripple::AccountID owner;
    std::optional<ripple::Blob> uri;
    bool isBurned = false;
    bool onlyUriChanged = false;  // Whether only the URI was changed

    /**
     * @brief Construct a new NFTsData object
     *
     * @note This constructor is used when parsing an NFTokenMint tx
     * Unfortunately because of the extreme edge case of being able to re-mint an NFT with the same
     * ID, we must explicitly record a null URI. For this reason, we _always_ write this field as a
     * result of this tx.
     *
     * @param tokenID The token ID
     * @param owner The owner
     * @param uri The URI
     * @param meta The transaction metadata
     */
    NFTsData(
        ripple::uint256 const& tokenID,
        ripple::AccountID const& owner,
        ripple::Blob const& uri,
        ripple::TxMeta const& meta
    )
        : tokenID(tokenID)
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , owner(owner)
        , uri(uri)
    {
    }

    /**
     * @brief Construct a new NFTsData object
     *
     * @note This constructor is used when parsing an NFTokenBurn or NFTokenAcceptOffer tx
     *
     * @param tokenID The token ID
     * @param owner The owner
     * @param meta The transaction metadata
     * @param isBurned Whether the NFT is burned
     */
    NFTsData(
        ripple::uint256 const& tokenID,
        ripple::AccountID const& owner,
        ripple::TxMeta const& meta,
        bool isBurned
    )
        : tokenID(tokenID)
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , owner(owner)
        , isBurned(isBurned)
    {
    }

    /**
     * @brief Construct a new NFTsData object
     *
     * @note This constructor is used when parsing an NFTokenPage directly from ledger state.
     * Unfortunately because of the extreme edge case of being able to re-mint an NFT with the same
     * ID, we must explicitly record a null URI. For this reason, we _always_ write this field as a
     * result of this tx.
     *
     * @param tokenID The token ID
     * @param ledgerSequence The ledger sequence
     * @param owner The owner
     * @param uri The URI
     */
    NFTsData(
        ripple::uint256 const& tokenID,
        std::uint32_t const ledgerSequence,
        ripple::AccountID const& owner,
        ripple::Blob const& uri
    )
        : tokenID(tokenID), ledgerSequence(ledgerSequence), owner(owner), uri(uri)
    {
    }

    /**
     * @brief Construct a new NFTsData object with only the URI changed
     *
     * @param tokenID The token ID
     * @param meta The transaction metadata
     * @param uri The new URI
     *
     */
    NFTsData(ripple::uint256 const& tokenID, ripple::TxMeta const& meta, ripple::Blob const& uri)
        : tokenID(tokenID)
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , uri(uri)
        , onlyUriChanged(true)
    {
    }
};

/**
 * @brief Represents an MPT and holder pair
 */
struct MPTHolderData {
    ripple::uint192 mptID;
    ripple::AccountID holder;
};

/**
 * @brief Represents a transaction link for an MPTokenIssuance.
 *
 * @note Writing one of these records inserts into two tables:
 * mptoken_issuance_transactions and account_mptoken_issuance_transactions.
 */
struct MPTokenIssuanceTransactionsData {
    ripple::uint192 mptIssuanceID;
    boost::container::flat_set<ripple::AccountID> accounts;
    std::uint32_t ledgerSequence{};
    std::uint32_t transactionIndex{};
    ripple::uint256 txHash;
};

/**
 * @brief Check whether the supplied object is a dir node.
 *
 * @param object The object to check
 * @return true if the object is a dir node; false otherwise
 */
template <typename T>
inline bool
isDirNode(T const& object)
{
    static constexpr auto kMinSizeRequired = 3;
    if (std::size(object) < kMinSizeRequired)
        return false;

    static constexpr short kDirNodeSpaceKey = 0x0064;
    short const spaceKey = (object.data()[1] << 8) | object.data()[2];
    return spaceKey == kDirNodeSpaceKey;
}

/**
 * @brief Check whether the supplied object is a book dir.
 *
 * @param key The key into the object
 * @param object The object to check
 * @return true if the object is a book dir; false otherwise
 */
template <typename T, typename R>
inline bool
isBookDir(T const& key, R const& object)
{
    if (!isDirNode(object))
        return false;

    ripple::STLedgerEntry const sle{ripple::SerialIter{object.data(), object.size()}, key};
    return !sle[~ripple::sfOwner].has_value();
}

/**
 * @brief Get the book base.
 *
 * @param key The key to get the book base out of
 * @return Book base as ripple::uint256
 */
template <typename T>
inline ripple::uint256
getBookBase(T const& key)
{
    static constexpr size_t kEySize = 24;

    ASSERT(key.size() == ripple::uint256::size(), "Invalid key size {}", key.size());

    ripple::uint256 ret;
    for (size_t i = 0; i < kEySize; ++i)
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
static constexpr std::uint32_t kRippleEpochStart = 946684800;
