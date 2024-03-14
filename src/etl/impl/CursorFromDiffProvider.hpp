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
#include "util/Assert.hpp"

#include <ripple/basics/base_uint.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <set>
#include <vector>

namespace etl::impl {

class CursorFromDiffProvider : public BaseCursorProvider {
    std::shared_ptr<BackendInterface> backend_;
    size_t numCursors_;

public:
    CursorFromDiffProvider(std::shared_ptr<BackendInterface> const& backend, size_t numCursors)
        : backend_{backend}, numCursors_{numCursors}
    {
    }

    [[nodiscard]] std::vector<CursorPair>
    getCursors(uint32_t const seq) const override
    {
        namespace rg = std::ranges;
        namespace vs = std::views;

        auto const fetchDiff = [this, seq](uint32_t offset) {
            return data::synchronousAndRetryOnTimeout([this, seq, offset](auto yield) {
                return backend_->fetchLedgerDiff(seq - offset, yield);
            });
        };

        auto const range = backend_->fetchLedgerRange();
        ASSERT(range.has_value(), "Ledger range is not available when cache is loading");

        std::set<ripple::uint256> liveCursors;
        std::set<ripple::uint256> deletedCursors;
        auto i = 0;
        while (liveCursors.size() < numCursors_ and seq - i >= range->minSequence) {
            auto diffs = fetchDiff(i++);
            rg::copy(
                diffs  //
                    | vs::filter([&deletedCursors](auto const& obj) {
                          return not obj.blob.empty() and !deletedCursors.contains(obj.key);
                      })  //
                    | vs::transform([](auto const& obj) { return obj.key; }),
                std::inserter(liveCursors, std::begin(liveCursors))
            );

            // track the deleted objects
            rg::copy(
                diffs                                                               //
                    | vs::filter([](auto const& obj) { return obj.blob.empty(); })  //
                    | vs::transform([](auto const& obj) { return obj.key; }),
                std::inserter(deletedCursors, std::begin(deletedCursors))
            );
        }

        std::vector<ripple::uint256> cursors{data::firstKey};
        rg::copy(liveCursors | vs::take(std::min(liveCursors.size(), numCursors_)), std::back_inserter(cursors));
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
            [](auto&& a, auto&& b) -> CursorPair {
                return {a, b};
            }
        );

        return pairs;
    }
};

}  // namespace etl::impl
