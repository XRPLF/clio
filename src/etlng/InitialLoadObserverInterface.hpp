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

#pragma once

#include "etlng/Models.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace etlng {

/**
 * @brief The interface for observing the initial ledger load
 */
struct InitialLoadObserverInterface {
    virtual ~InitialLoadObserverInterface() = default;

    /**
     * @brief Callback for each incoming batch of objects during initial ledger load
     *
     * @param seq The sequence for this batch of objects
     * @param data The batch of objects
     * @param lastKey The last key of the previous batch if there was one
     */
    virtual void
    onInitialLoadGotMoreObjects(
        uint32_t seq,
        std::vector<model::Object> const& data,
        std::optional<std::string> lastKey = std::nullopt
    ) = 0;
};

}  // namespace etlng
