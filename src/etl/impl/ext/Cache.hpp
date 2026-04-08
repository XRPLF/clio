#pragma once

#include "etl/CacheUpdaterInterface.hpp"
#include "etl/Models.hpp"
#include "etl/impl/CacheUpdater.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace etl::impl {

class CacheExt {
    std::shared_ptr<CacheUpdaterInterface> cacheUpdater_;

    util::Logger log_{"ETL"};

public:
    CacheExt(std::shared_ptr<CacheUpdaterInterface> cacheUpdater);

    void
    onLedgerData(model::LedgerData const& data);

    void
    onInitialData(model::LedgerData const& data);

    void
    onInitialObjects(
        uint32_t seq,
        std::vector<model::Object> const& objs,
        [[maybe_unused]] std::string lastKey
    );

    // We want cache updates through ETL if we are a potential writer but currently are not writing
    // to DB
    static bool
    allowInReadonly()
    {
        return true;
    }
};

}  // namespace etl::impl
