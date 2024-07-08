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

#include "data/LedgerCache.hpp"

#include "data/Types.hpp"
#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace data {

uint32_t
LedgerCache::latestLedgerSequence() const
{
    std::shared_lock const lck{mtx_};
    return latestSeq_;
}

void
LedgerCache::waitUntilCacheContainsSeq(uint32_t seq)
{
    if (disabled_)
        return;

    std::unique_lock lock(mtx_);
    cv_.wait(lock, [this, seq] { return latestSeq_ >= seq; });
    return;
}

void
LedgerCache::update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground)
{
    if (disabled_)
        return;

    {
        std::scoped_lock const lck{mtx_};
        if (seq > latestSeq_) {
            ASSERT(
                seq == latestSeq_ + 1 || latestSeq_ == 0,
                "New sequense must be either next or first. seq = {}, latestSeq_ = {}",
                seq,
                latestSeq_
            );
            latestSeq_ = seq;
        }
        for (auto const& obj : objs) {
            if (!obj.blob.empty()) {
                if (isBackground && deletes_.contains(obj.key))
                    continue;

                auto& e = map_[obj.key];
                if (seq > e.seq) {
                    e = {seq, obj.blob};
                }
            } else {
                map_.erase(obj.key);
                if (!full_ && !isBackground)
                    deletes_.insert(obj.key);
            }
        }
        cv_.notify_all();
    }
}

std::optional<LedgerObject>
LedgerCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_ or not full_)
        return {};

    std::shared_lock const lck{mtx_};
    ++successorReqCounter_.get();
    if (seq != latestSeq_)
        return {};
    auto e = map_.upper_bound(key);
    if (e == map_.end())
        return {};
    ++successorHitCounter_.get();
    return {{e->first, e->second.blob}};
}

std::optional<LedgerObject>
LedgerCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_ or not full_)
        return {};

    std::shared_lock const lck{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.lower_bound(key);
    if (e == map_.begin())
        return {};
    --e;
    return {{e->first, e->second.blob}};
}

std::optional<Blob>
LedgerCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_)
        return {};

    std::shared_lock const lck{mtx_};
    if (seq > latestSeq_)
        return {};
    ++objectReqCounter_.get();
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    ++objectHitCounter_.get();
    return {e->second.blob};
}

void
LedgerCache::setDisabled()
{
    disabled_ = true;
}

bool
LedgerCache::isDisabled() const
{
    return disabled_;
}

void
LedgerCache::setFull()
{
    if (disabled_)
        return;

    full_ = true;
    std::scoped_lock const lck{mtx_};
    deletes_.clear();
}

bool
LedgerCache::isFull() const
{
    return full_;
}

size_t
LedgerCache::size() const
{
    std::shared_lock const lck{mtx_};
    return map_.size();
}

float
LedgerCache::getObjectHitRate() const
{
    if (objectReqCounter_.get().value() == 0u)
        return 1;
    return static_cast<float>(objectHitCounter_.get().value()) / objectReqCounter_.get().value();
}

float
LedgerCache::getSuccessorHitRate() const
{
    if (successorReqCounter_.get().value() == 0u)
        return 1;
    return static_cast<float>(successorHitCounter_.get().value()) / successorReqCounter_.get().value();
}

}  // namespace data
