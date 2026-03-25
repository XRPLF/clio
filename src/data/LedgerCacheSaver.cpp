#include "data/LedgerCacheSaver.hpp"

#include "data/LedgerCacheInterface.hpp"
#include "util/Assert.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <string>
#include <thread>

namespace data {

LedgerCacheSaver::LedgerCacheSaver(
    util::config::ClioConfigDefinition const& config,
    LedgerCacheInterface const& cache
)
    : cacheFilePath_(config.maybeValue<std::string>("cache.file.path"))
    , cache_(cache)
    , isAsync_(config.get<bool>("cache.file.async_save"))
{
}

LedgerCacheSaver::~LedgerCacheSaver()
{
    waitToFinish();
}

void
LedgerCacheSaver::save()
{
    ASSERT(not savingThread_.has_value(), "Multiple save() calls are not allowed");
    savingThread_ = std::thread([this]() {
        if (not cacheFilePath_.has_value()) {
            return;
        }

        LOG(util::LogService::info()) << "Saving ledger cache to " << *cacheFilePath_;
        if (auto const [success, durationMs] =
                util::timed([&]() { return cache_.get().saveToFile(*cacheFilePath_); });
            success.has_value()) {
            LOG(util::LogService::info())
                << "Successfully saved ledger cache in " << durationMs << " ms";
        } else {
            LOG(util::LogService::error())
                << "Error saving LedgerCache to file: " << success.error();
        }
    });
    if (not isAsync_) {
        waitToFinish();
    }
}

void
LedgerCacheSaver::waitToFinish()
{
    if (savingThread_.has_value() and savingThread_->joinable()) {
        savingThread_->join();
    }
    savingThread_.reset();
}

}  // namespace data
