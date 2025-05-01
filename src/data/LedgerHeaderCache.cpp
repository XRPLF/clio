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

#include "data/LedgerHeaderCache.hpp"

#include "util/Mutex.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>

namespace data {

FetchLedgerCache::FetchLedgerCache()
    : mutexPtr_{std::make_unique<util::Mutex<std::optional<CacheEntry>, std::shared_mutex>>()}
{
}

void
FetchLedgerCache::put(CacheEntry const& cacheEntry) const
{
    auto lock = mutexPtr_->lock<std::unique_lock>();
    *lock = cacheEntry;
}

std::optional<FetchLedgerCache::CacheEntry>
FetchLedgerCache::get() const
{
    auto const lock = mutexPtr_->lock<std::shared_lock>();
    return lock.get();
}

}  // namespace data
