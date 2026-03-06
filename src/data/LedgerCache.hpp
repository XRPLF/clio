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

#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/Models.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/hardened_hash.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace data {

/**
 * @brief Cache for an entire ledger.
 */
class LedgerCache : public LedgerCacheInterface {
public:
    /** @brief An entry of the cache */
    struct CacheEntry {
        uint32_t seq = 0;
        Blob blob;
    };

    using CacheMap = std::map<ripple::uint256, CacheEntry>;

private:
    // counters for fetchLedgerObject(s) hit rate
    std::reference_wrapper<util::prometheus::CounterInt> objectReqCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "request"}, {"fetch", "ledger_objects"}}),
            "LedgerCache statistics"
        )
    };
    std::reference_wrapper<util::prometheus::CounterInt> objectHitCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "ledger_objects"}})
        )
    };

    // counters for fetchSuccessorKey hit rate
    std::reference_wrapper<util::prometheus::CounterInt> successorReqCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "request"}, {"fetch", "successor_key"}}),
            "ledgerCache"
        )
    };
    std::reference_wrapper<util::prometheus::CounterInt> successorHitCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "successor_key"}})
        )
    };

    CacheMap map_;
    CacheMap deleted_;

    mutable std::shared_mutex mtx_;
    std::condition_variable_any cv_;
    uint32_t latestSeq_ = 0;
    util::prometheus::Bool full_{PrometheusService::boolMetric(
        "ledger_cache_full",
        util::prometheus::Labels{},
        "Whether ledger cache full or not"
    )};
    util::prometheus::Bool disabled_{PrometheusService::boolMetric(
        "ledger_cache_disabled",
        util::prometheus::Labels{},
        "Whether ledger cache is disabled or not"
    )};
    util::prometheus::Bool isCurrentlyLoading_{
        PrometheusService::boolMetric(
            "ledger_cache_is_currently_loading",
            util::prometheus::Labels{},
            "Whether ledger cache is currently loading or not"
        )

    };

    // temporary set to prevent background thread from writing already deleted data. not used when
    // cache is full
    std::unordered_set<ripple::uint256, ripple::hardened_hash<>> deletes_;

public:
    void
    update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground) override;

    void
    update(std::vector<etl::model::Object> const& objs, uint32_t seq) override;

    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const override;

    std::optional<Blob>
    getDeleted(ripple::uint256 const& key, uint32_t seq) const override;

    std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const override;

    std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const override;

    void
    setDisabled() override;

    bool
    isDisabled() const override;

    void
    setFull() override;

    uint32_t
    latestLedgerSequence() const override;

    bool
    isFull() const override;

    size_t
    size() const override;

    float
    getObjectHitRate() const override;

    float
    getSuccessorHitRate() const override;

    void
    waitUntilCacheContainsSeq(uint32_t seq) override;

    std::expected<void, std::string>
    saveToFile(std::string const& path) const override;

    std::expected<void, std::string>
    loadFromFile(std::string const& path, uint32_t minLatestSequence) override;

    void
    startLoading() override;

    [[nodiscard]] bool
    isCurrentlyLoading() const override;
};

}  // namespace data
