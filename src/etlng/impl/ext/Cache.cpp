//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etlng/impl/ext/Cache.hpp"

#include "data/LedgerCacheInterface.hpp"
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace etlng::impl {

CacheExt::CacheExt(data::LedgerCacheInterface& cache) : cache_(cache)
{
}

void
CacheExt::onLedgerData(model::LedgerData const& data) const
{
    cache_.get().update(data.objects, data.seq);
    LOG(log_.trace()) << "got data. objects cnt = " << data.objects.size();
}

void
CacheExt::onInitialData(model::LedgerData const& data) const
{
    LOG(log_.trace()) << "got initial data. objects cnt = " << data.objects.size();
    cache_.get().update(data.objects, data.seq);
    cache_.get().setFull();
}

void
CacheExt::onInitialObjects(uint32_t seq, std::vector<model::Object> const& objs, [[maybe_unused]] std::string lastKey)
    const
{
    LOG(log_.trace()) << "got initial objects cnt = " << objs.size();
    cache_.get().update(objs, seq);
}

}  // namespace etlng::impl
