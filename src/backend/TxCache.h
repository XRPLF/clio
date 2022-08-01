#ifndef CLIO_TXCACHE_H_INCLUDED
#define CLIO_TXCACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <ripple/basics/hardened_hash.h>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#define NUM_LEDGERS_CACHED 10

namespace Backend {
class TxCache
{
    // basically a ring buffer
    // array of maps of transaction hashes -> TransactionAndMetadata objects
    // replace the oldest ledger in cache when inserting a new ledger
    std::array<
        std::map<ripple::uint256, TransactionAndMetadata>,
        NUM_LEDGERS_CACHED>
        cache_;
    mutable std::shared_mutex mtx_;
    uint32_t latestSeq_ = 0;
    std::atomic_int tail_;
    mutable std::atomic_int txReqCounter_;
    mutable std::atomic_int txHitCounter_;

public:
    // Update the cache with new ledger objects
    void
    update(
        std::vector<ripple::uint256> const& hashes,
        std::vector<TransactionAndMetadata> const& transactions,
        uint32_t seq);

    std::optional<TransactionAndMetadata>
    get(ripple::uint256 const& key) const;

    std::optional<std::vector<TransactionAndMetadata>>
    getLedgerTransactions(std::uint32_t const ledgerSequence) const;

    std::optional<std::vector<ripple::uint256>>
    getLedgerTransactionHashes(std::uint32_t const ledgerSequence) const;

    uint32_t
    latestLedgerSequence() const;

    float
    getHitRate() const;
};

}  // namespace Backend
#endif
