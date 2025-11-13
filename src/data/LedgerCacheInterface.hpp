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

#include "data/Types.hpp"
#include "etl/Models.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/hardened_hash.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace data {

/**
 * @brief Cache for an entire ledger.
 */
class LedgerCacheInterface {
public:
    virtual ~LedgerCacheInterface() = default;
    LedgerCacheInterface() = default;
    LedgerCacheInterface(LedgerCacheInterface&&) = delete;
    LedgerCacheInterface(LedgerCacheInterface const&) = delete;
    LedgerCacheInterface&
    operator=(LedgerCacheInterface&&) = delete;
    LedgerCacheInterface&
    operator=(LedgerCacheInterface const&) = delete;

    /**
     * @brief Update the cache with new ledger objects.
     *
     * @param objs The ledger objects to update cache with
     * @param seq The sequence to update cache for
     * @param isBackground Should be set to true when writing old data from a background thread
     */
    virtual void
    update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground = false) = 0;

    /**
     * @brief Update the cache with new ledger objects.
     *
     * @param objs The ledger objects to update cache with
     * @param seq The sequence to update cache for
     */
    virtual void
    update(std::vector<etl::model::Object> const& objs, uint32_t seq) = 0;

    /**
     * @brief Fetch a cached object by its key and sequence number.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached Blob; otherwise nullopt is returned
     */
    virtual std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const = 0;

    /**
     * @brief Fetch a recently deleted object by its key and sequence number.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in deleted cache, will return the cached Blob; otherwise nullopt is returned
     */
    virtual std::optional<Blob>
    getDeleted(ripple::uint256 const& key, uint32_t seq) const = 0;

    /**
     * @brief Gets a cached successor.
     *
     * Note: This function always returns std::nullopt when @ref isFull() returns false.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached successor; otherwise nullopt is returned
     */
    virtual std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const = 0;

    /**
     * @brief Gets a cached predcessor.
     *
     * Note: This function always returns std::nullopt when @ref isFull() returns false.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached predcessor; otherwise nullopt is returned
     */
    virtual std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const = 0;

    /**
     * @brief Disables the cache.
     */
    virtual void
    setDisabled() = 0;

    /**
     * @return true if the cache is disabled; false otherwise
     */
    virtual bool
    isDisabled() const = 0;

    /**
     * @brief Sets the full flag to true.
     *
     * This is used when cache loaded in its entirety at startup of the application. This can be either loaded from DB,
     * populated together with initial ledger download (on first run) or downloaded from a peer node (specified in
     * config).
     */
    virtual void
    setFull() = 0;

    /**
     * @return The latest ledger sequence for which cache is available.
     */
    virtual uint32_t
    latestLedgerSequence() const = 0;

    /**
     * @return true if the cache has all data for the most recent ledger; false otherwise
     */
    virtual bool
    isFull() const = 0;

    /**
     * @return The total size of the cache.
     */
    virtual size_t
    size() const = 0;

    /**
     * @return A number representing the success rate of hitting an object in the cache versus missing it.
     */
    virtual float
    getObjectHitRate() const = 0;

    /**
     * @return A number representing the success rate of hitting a successor in the cache versus missing it.
     */
    virtual float
    getSuccessorHitRate() const = 0;

    /**
     * @brief Waits until the cache contains a specific sequence.
     *
     * @param seq The sequence to wait for
     */
    virtual void
    waitUntilCacheContainsSeq(uint32_t seq) = 0;
};

}  // namespace data
