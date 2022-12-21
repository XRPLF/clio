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
#include <ripple/basics/hardened_hash.h>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace clio::data {

class SimpleCache
{
    struct CacheEntry
    {
        uint32_t seq = 0;
        Blob blob;
    };

    // counters for fetchLedgerObject(s) hit rate
    mutable std::atomic_uint32_t objectReqCounter_;
    mutable std::atomic_uint32_t objectHitCounter_;
    // counters for fetchSuccessorKey hit rate
    mutable std::atomic_uint32_t successorReqCounter_;
    mutable std::atomic_uint32_t successorHitCounter_;

    std::map<ripple::uint256, CacheEntry> map_;
    mutable std::shared_mutex mtx_;
    uint32_t latestSeq_ = 0;
    std::atomic_bool full_ = false;
    std::atomic_bool disabled_ = false;
    // temporary set to prevent background thread from writing already deleted
    // data. not used when cache is full
    std::unordered_set<ripple::uint256, ripple::hardened_hash<>> deletes_;

public:
    // Update the cache with new ledger objects
    // set isBackground to true when writing old data from a background thread
    void
    update(
        std::vector<LedgerObject> const& blobs,
        uint32_t seq,
        bool isBackground = false);

    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const;

    // always returns empty optional if isFull() is false
    std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const;

    // always returns empty optional if isFull() is false
    std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const;

    void
    setDisabled();

    void
    setFull();

    uint32_t
    latestLedgerSequence() const;

    // whether the cache has all data for the most recent ledger
    bool
    isFull() const;

    size_t
    size() const;

    float
    getObjectHitRate() const;

    float
    getSuccessorHitRate() const;
};

}  // namespace clio::data
