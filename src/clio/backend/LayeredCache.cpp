#include <clio/backend/LayeredCache.h>
namespace Backend {

void
LayeredCache::insert(
    ripple::uint256 const& key,
    Blob const& value,
    uint32_t seq)
{
    auto entry = map_[key];
    // stale insert, do nothing
    if (seq <= entry.recent.seq)
        return;
    entry.old = entry.recent;
    entry.recent = {seq, value};
    if (value.empty())
        pendingDeletes_.push_back(key);
    if (!entry.old.blob.empty())
        pendingSweeps_.push_back(key);
}

std::optional<Blob>
LayeredCache::select(CacheEntry const& entry, uint32_t seq) const
{
    if (seq < entry.old.seq)
        return {};
    if (seq < entry.recent.seq && !entry.old.blob.empty())
        return entry.old.blob;
    if (!entry.recent.blob.empty())
        return entry.recent.blob;
    return {};
}
void
LayeredCache::update(std::vector<LedgerObject> const& blobs, uint32_t seq)
{
    std::unique_lock lck{mtx_};
    if (seq > mostRecentSequence_)
        mostRecentSequence_ = seq;
    for (auto const& k : pendingSweeps_)
    {
        auto e = map_[k];
        e.old = {};
    }
    for (auto const& k : pendingDeletes_)
    {
        map_.erase(k);
    }
    for (auto const& b : blobs)
    {
        insert(b.key, b.blob, seq);
    }
}
std::optional<LedgerObject>
LayeredCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    ripple::uint256 curKey = key;
    while (true)
    {
        std::shared_lock lck{mtx_};
        if (seq < mostRecentSequence_ - 1)
            return {};
        auto e = map_.upper_bound(curKey);
        if (e == map_.end())
            return {};
        auto const& entry = e->second;
        auto blob = select(entry, seq);
        if (!blob)
        {
            curKey = e->first;
            continue;
        }
        else
            return {{e->first, *blob}};
    }
}
std::optional<LedgerObject>
LayeredCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    ripple::uint256 curKey = key;
    std::shared_lock lck{mtx_};
    while (true)
    {
        if (seq < mostRecentSequence_ - 1)
            return {};
        auto e = map_.lower_bound(curKey);
        --e;
        if (e == map_.begin())
            return {};
        auto const& entry = e->second;
        auto blob = select(entry, seq);
        if (!blob)
        {
            curKey = e->first;
            continue;
        }
        else
            return {{e->first, *blob}};
    }
}
std::optional<Blob>
LayeredCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck{mtx_};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    auto const& entry = e->second;
    return select(entry, seq);
}
}  // namespace Backend
