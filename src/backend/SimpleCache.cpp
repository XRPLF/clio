#include <ripple/protocol/STLedgerEntry.h>
#include <backend/SimpleCache.h>
namespace Backend {
uint32_t
SimpleCache::latestLedgerSequence() const
{
    std::shared_lock lck{mtx_};
    return latestSeq_;
}

void
SimpleCache::update(
    std::vector<LedgerObject> const& objs,
    uint32_t seq,
    bool isBackground)
{
    {
        std::unique_lock lck{mtx_};
        if (seq > latestSeq_)
        {
            assert(seq == latestSeq_ + 1 || latestSeq_ == 0);
            latestSeq_ = seq;
        }
        for (auto const& obj : objs)
        {
            if (obj.blob.size())
            {
                if (isBackground && deletes_.count(obj.key))
                    continue;
                auto& e = map_[obj.key];
                if (seq > e.seq)
                {
                    // we don't need to worry about hasJson and updatingJson
                    // here, because we have a unique_lock, so no other threads
                    // can be touching the cache
                    e = {seq, obj.blob, false, false, {}};
                }
            }
            else
            {
                map_.erase(obj.key);
                if (!full_ && !isBackground)
                    deletes_.insert(obj.key);
            }
        }
    }
    bool updateJsonCache =
        isBackground ? jsonCaching_ == FULL : jsonCaching_ == DIFFS;
    if (updateJsonCache)
    {
        for (auto const& obj : objs)
        {
            if (obj.blob.size())
            {
                ripple::STLedgerEntry sle{
                    ripple::SerialIter{obj.blob.data(), obj.blob.size()},
                    obj.key};
                boost::json::value json = boost::json::parse(
                    sle.getJson(ripple::JsonOptions::none).toStyledString());
                updateJson(obj.key, seq, std::move(json.as_object()));
            }
        }
    }
}
std::optional<LedgerObject>
SimpleCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (!full_)
        return {};
    std::shared_lock{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.upper_bound(key);
    if (e == map_.end())
        return {};
    return {{e->first, e->second.blob}};
}
std::optional<LedgerObject>
SimpleCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (!full_)
        return {};
    std::shared_lock lck{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.lower_bound(key);
    if (e == map_.begin())
        return {};
    --e;
    return {{e->first, e->second.blob}};
}
std::optional<Blob>
SimpleCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck{mtx_};
    if (seq > latestSeq_)
        return {};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    return {e->second.blob};
}
std::optional<boost::json::object>
SimpleCache::getJson(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck{mtx_};
    if (jsonCaching_ == NONE)
        return {};
    if (seq > latestSeq_)
        return {};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    if (!e->second.hasJson)
        return {};
    return {e->second.json};
}
void
SimpleCache::updateJson(
    ripple::uint256 const& key,
    uint32_t seq,
    boost::json::object&& json) const
{
    std::shared_lock lck{mtx_};
    if (jsonCaching_ == NONE)
        return;
    if (seq > latestSeq_)
        return;
    if (!map_.count(key))
        return;
    auto& e = const_cast<SimpleCache::CacheEntry&>(map_.at(key));
    if (seq < e.seq)
        return;
    if (e.hasJson)
        return;
    if (!e.updatingJson.exchange(true))
    {
        e.json = std::move(json);
        e.hasJson = true;
    }
    e.updatingJson = false;
}

void
SimpleCache::enableJsonCaching(bool full)
{
    std::unique_lock lck{mtx_};
    jsonCaching_ = full ? FULL : DIFFS;
}

void
SimpleCache::setFull()
{
    full_ = true;
    std::unique_lock lck{mtx_};
    deletes_.clear();
}

bool
SimpleCache::isFull() const
{
    return full_;
}
size_t
SimpleCache::size() const
{
    std::shared_lock lck{mtx_};
    return map_.size();
}
}  // namespace Backend
