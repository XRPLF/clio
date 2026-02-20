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
