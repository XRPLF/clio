#include <backend/TxCache.h>
namespace Backend {

uint32_t
TxCache::latestLedgerSequence() const
{
    std::shared_lock lck{mtx_};
    return latestSeq_;
}

// adds an entire ledger's worth of transactions to the cache. Evicts oldest
// ledger.
void
TxCache::update(
    std::vector<ripple::uint256> const& hashes,
    std::vector<TransactionAndMetadata> const& transactions,
    uint32_t seq)
{
    std::unique_lock lck{mtx_};
    cache_[tail_].clear();

    assert(seq == latestSeq_ + 1 || latestSeq_ == 0);
    latestSeq_ = seq;
    for (std::size_t i = 0; i < hashes.size(); i++)
    {
        cache_[tail_][hashes[i]] = transactions[i];
    }
    tail_ = (tail_ + 1) % NUM_LEDGERS_CACHED;
}

std::optional<TransactionAndMetadata>
TxCache::get(ripple::uint256 const& key) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;

    for (auto& map : cache_)
    {
        auto e = map.find(key);
        if (e != map.end())
        {
            txHitCounter_++;
            return {e->second};
        }
    }
    return {};
}

float
TxCache::getHitRate() const
{
    if (!txReqCounter_)
        return 0;
    return ((float)txHitCounter_ / txReqCounter_);
}

std::optional<std::vector<TransactionAndMetadata>>
TxCache::getLedgerTransactions(std::uint32_t const ledgerSequence) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;
    auto diff = latestSeq_ - ledgerSequence;
    if (diff < NUM_LEDGERS_CACHED)
    {
        auto head = (tail_ - 1) % NUM_LEDGERS_CACHED;
        auto ledgerCache = cache_[(head - diff) % NUM_LEDGERS_CACHED];
        std::vector<TransactionAndMetadata> result;
        for (auto const& tx : ledgerCache)
        {
            result.push_back(tx.second);
        }
        if (result.size() > 0)
        {
            txHitCounter_++;
            return {result};
        }
    }
    return {};
}

std::optional<std::vector<ripple::uint256>>
TxCache::getLedgerTransactionHashes(std::uint32_t const ledgerSequence) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;
    auto diff = latestSeq_ - ledgerSequence;
    if (diff < NUM_LEDGERS_CACHED)
    {
        auto head = (tail_ - 1) % NUM_LEDGERS_CACHED;
        auto ledgerCache = cache_[(head - diff) % NUM_LEDGERS_CACHED];
        std::vector<ripple::uint256> result;
        for (auto const& tx : ledgerCache)
        {
            result.push_back(tx.first);
        }
        if (result.size() > 0)
        {
            txHitCounter_++;
            return {result};
        }
    }
    return {};
}

}  // namespace Backend
