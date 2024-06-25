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

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "etl/impl/BaseCursorProvider.hpp"

#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

namespace etl::impl {

class CursorFromAccountProvider : public BaseCursorProvider {
    std::shared_ptr<BackendInterface> backend_;
    size_t numCursors_;
    size_t pageSize_;

public:
    CursorFromAccountProvider(std::shared_ptr<BackendInterface> const& backend, size_t numCursors, size_t pageSize)
        : backend_{backend}, numCursors_{numCursors}, pageSize_{pageSize}
    {
    }

    [[nodiscard]] std::vector<CursorPair>
    getCursors(uint32_t const seq) const override
    {
        namespace rg = std::ranges;

        auto accountRoots = [this, seq]() {
            return data::synchronousAndRetryOnTimeout([this, seq](auto yield) {
                return backend_->fetchAccountRoots(numCursors_, pageSize_, seq, yield);
            });
        }();

        rg::sort(accountRoots);
        std::vector<ripple::uint256> cursors{data::firstKey};
        rg::copy(accountRoots.begin(), accountRoots.end(), std::back_inserter(cursors));
        rg::sort(cursors);
        cursors.push_back(data::lastKey);

        std::vector<CursorPair> pairs;
        pairs.reserve(cursors.size());

        // FIXME: this should be `cursors | vs::pairwise` (C++23)
        std::transform(
            std::begin(cursors),
            std::prev(std::end(cursors)),
            std::next(std::begin(cursors)),
            std::back_inserter(pairs),
            [](auto&& a, auto&& b) -> CursorPair { return {a, b}; }
        );

        return pairs;
    }
};

}  // namespace etl::impl
