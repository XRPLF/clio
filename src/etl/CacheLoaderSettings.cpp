//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "etl/CacheLoaderSettings.hpp"

#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <cstddef>
#include <cstdint>

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
make_CacheLoaderSettings(util::config::ClioConfigDefinition const& config)
{
    CacheLoaderSettings settings;
    settings.numThreads = config.getValue("io_threads").asIntType<uint16_t>();
    auto const cache = config.getObject("cache");
    // Given diff number to generate cursors
    settings.numCacheDiffs = cache.getValue("num_diffs").asIntType<std::size_t>();
    // Given cursors number fetching from diff
    settings.numCacheCursorsFromDiff = cache.getValue("num_cursors_from_diff").asIntType<std::size_t>();
    // Given cursors number fetching from account
    settings.numCacheCursorsFromAccount = cache.getValue("num_cursors_from_account").asIntType<std::size_t>();

    settings.numCacheMarkers = cache.getValue("num_markers").asIntType<std::size_t>();
    settings.cachePageFetchSize = cache.getValue("page_fetch_size").asIntType<std::size_t>();

    auto const entry = cache.getValue("load").asString();
    if (boost::iequals(entry, "sync"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::SYNC;
    if (boost::iequals(entry, "async"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::ASYNC;
    if (boost::iequals(entry, "none") or boost::iequals(entry, "no"))
        settings.loadStyle = CacheLoaderSettings::LoadStyle::NONE;

    return settings;
}

}  // namespace etl
