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

#include "data/LedgerHeaderCacheInterface.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>

namespace data::cassandra {

/**
 * @brief A simple cache holding one `ripple::LedgerHeader` to reduce DB lookups.
 *
 * Used internally by backend implementations. When a ledger header is
 * fetched via `FetchLedgerBySeq` (often triggered by RPC commands),
 * the result can be stored here. Subsequent requests for the same ledger
 * sequence can then retrieve the header from this cache, avoiding unnecessary
 * database reads and improving performance.
 */
class FetchLedgerCache : public LedgerHeaderCacheInterface {
public:
    void
    setLedgerHeader(ripple::LedgerHeader const& ledgerHeader) override
    {
        ledger_ = ledgerHeader;
    }

    void
    setSeq(uint32_t const seq) override
    {
        seq_ = seq;
    }

    std::optional<ripple::LedgerHeader>
    getLedgerHeader() const override
    {
        return ledger_;
    }

    uint32_t
    getSeq() const override
    {
        return seq_;
    }

private:
    std::optional<ripple::LedgerHeader> ledger_;
    uint32_t seq_{};
};

}  // namespace data::cassandra
