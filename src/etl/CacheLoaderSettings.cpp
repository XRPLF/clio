#include "etl/CacheLoaderSettings.hpp"

#include "util/config/ConfigDefinition.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace etl {

[[nodiscard]] bool
CacheLoaderSettings::isSync() const
{
    return loadStyle == LoadStyle::SYNC;
}

[[nodiscard]] bool
CacheLoaderSettings::isAsync() const
{
    return loadStyle == LoadStyle::ASYNC;
}

[[nodiscard]] bool
CacheLoaderSettings::isDisabled() const
{
    return loadStyle == LoadStyle::NONE;
}

[[nodiscard]] CacheLoaderSettings
makeCacheLoaderSettings(util::config::ClioConfigDefinition const& config)
{
    CacheLoaderSettings settings;
    settings.numThreads = config.get<uint16_t>("io_threads");
    auto const cache = config.getObject("cache");
    // Given diff number to generate cursors
    settings.numCacheDiffs = cache.get<std::size_t>("num_diffs");
    // Given cursors number fetching from diff
    settings.numCacheCursorsFromDiff = cache.get<std::size_t>("num_cursors_from_diff");
    // Given cursors number fetching from account
    settings.numCacheCursorsFromAccount = cache.get<std::size_t>("num_cursors_from_account");

    settings.numCacheMarkers = cache.get<std::size_t>("num_markers");
    settings.cachePageFetchSize = cache.get<std::size_t>("page_fetch_size");

    if (auto filePath = cache.maybeValue<std::string>("file.path"); filePath.has_value()) {
        settings.cacheFileSettings = CacheLoaderSettings::CacheFileSettings{
            .path = std::move(filePath).value(),
            .maxAge = cache.get<uint32_t>("file.max_sequence_age")
        };
    }

    auto const entry = cache.get<std::string>("load");
    if (boost::iequals(entry, "sync"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::SYNC;
    if (boost::iequals(entry, "async"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::ASYNC;
    if (boost::iequals(entry, "none") or boost::iequals(entry, "no"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::NONE;

    return settings;
}

}  // namespace etl
