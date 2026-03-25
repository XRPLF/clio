#pragma once

#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/CacheUpdaterInterface.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace etl::impl {

class CacheUpdater : public CacheUpdaterInterface {
    std::reference_wrapper<data::LedgerCacheInterface> cache_;

    util::Logger log_{"ETL"};

public:
    CacheUpdater(data::LedgerCacheInterface& cache) : cache_{cache}
    {
    }

    void
    update(model::LedgerData const& data) override
    {
        cache_.get().update(data.objects, data.seq);
    }

    void
    update(uint32_t seq, std::vector<data::LedgerObject> const& objs) override
    {
        cache_.get().update(objs, seq);
    }

    void
    update(uint32_t seq, std::vector<model::Object> const& objs) override
    {
        cache_.get().update(objs, seq);
    }

    void
    setFull() override
    {
        cache_.get().setFull();
    }
};

}  // namespace etl::impl
