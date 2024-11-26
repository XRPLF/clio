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

#include <cstdint>
#include <optional>

namespace etlng {

/**
 * @brief An interface for the Extractor
 */
struct ExtractorInterface {
    virtual ~ExtractorInterface() = default;

    /**
     * @brief Extract diff data for a particular ledger
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger data diff between sequence and parent if available
     */
    [[nodiscard]] virtual std::optional<model::LedgerData>
    extractLedgerWithDiff(uint32_t seq) = 0;

    /**
     * @brief Extract data for a particular ledger
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger header and transaction+metadata blobs if available
     */
    [[nodiscard]] virtual std::optional<model::LedgerData>
    extractLedgerOnly(uint32_t seq) = 0;
};

}  // namespace etlng
