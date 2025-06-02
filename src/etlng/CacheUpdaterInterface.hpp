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

#include "data/Types.hpp"
#include "etlng/Models.hpp"

#include <cstdint>
#include <vector>

namespace etlng {

/**
 * @brief An interface for the Cache Updater
 */
struct CacheUpdaterInterface {
    virtual ~CacheUpdaterInterface() = default;

    /**
     * @brief Update the cache with ledger data
     * @param data The ledger data to update with
     */
    virtual void
    update(model::LedgerData const& data) = 0;

    /**
     * @brief Update the cache with ledger objects at a specific sequence
     * @param seq The ledger sequence number
     * @param objs The ledger objects to update with
     */
    virtual void
    update(uint32_t seq, std::vector<data::LedgerObject> const& objs) = 0;

    /**
     * @brief Update the cache with model objects at a specific sequence
     * @param seq The ledger sequence number
     * @param objs The model objects to update with
     */
    virtual void
    update(uint32_t seq, std::vector<model::Object> const& objs) = 0;

    /**
     * @brief Mark the cache as fully loaded
     */
    virtual void
    setFull() = 0;
};

}  // namespace etlng
