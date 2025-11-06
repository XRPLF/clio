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
