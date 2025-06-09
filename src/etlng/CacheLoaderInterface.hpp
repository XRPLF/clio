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

#include <cstdint>

namespace etlng {

/**
 * @brief An interface for the Cache Loader
 */
struct CacheLoaderInterface {
    virtual ~CacheLoaderInterface() = default;

    /**
     * @brief Load the cache with the most recent ledger data
     *
     * @param seq The sequence number of the ledger to load
     */
    virtual void
    load(uint32_t const seq) = 0;

    /**
     * @brief Stop the cache loading process
     */
    virtual void
    stop() noexcept = 0;

    /**
     * @brief Wait for all cache loading tasks to complete
     */
    virtual void
    wait() noexcept = 0;
};

}  // namespace etlng
