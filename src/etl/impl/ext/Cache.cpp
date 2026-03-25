#include "etl/impl/ext/Cache.hpp"

#include "etl/CacheUpdaterInterface.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

CacheExt::CacheExt(std::shared_ptr<CacheUpdaterInterface> cacheUpdater)
    : cacheUpdater_(std::move(cacheUpdater))
{
}

void
CacheExt::onLedgerData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got data. objects cnt = " << data.objects.size();
    cacheUpdater_->update(data);
}

void
CacheExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got initial data. objects cnt = " << data.objects.size();
    cacheUpdater_->update(data);
    cacheUpdater_->setFull();
}

void
CacheExt::onInitialObjects(
    uint32_t seq,
    std::vector<model::Object> const& objs,
    [[maybe_unused]] std::string lastKey
)
{
    LOG(log_.trace()) << "got initial objects cnt = " << objs.size();
    cacheUpdater_->update(seq, objs);
}

}  // namespace etl::impl
