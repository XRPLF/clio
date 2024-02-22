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
#include "util/log/Logger.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

namespace etl::impl {

struct CursorPair {
    ripple::uint256 start;
    ripple::uint256 end;
};

class CursorProvider {
    util::Logger log_{"ETL"};
    std::shared_ptr<BackendInterface> backend_;

    size_t numDiffs_;

public:
    CursorProvider(std::shared_ptr<BackendInterface> const& backend, size_t numDiffs)
        : backend_{backend}, numDiffs_{numDiffs}
    {
    }

    [[nodiscard]] std::vector<CursorPair>
    getCursors(uint32_t const seq) const
    {
        namespace rg = std::ranges;
        namespace vs = std::views;

        auto diffs = std::vector<data::LedgerObject>{};

        auto const append = [](auto&& a, auto&& b) { a.insert(std::end(a), std::begin(b), std::end(b)); };
        auto const fetchDiff = [this, seq](uint32_t offset) {
            return data::synchronousAndRetryOnTimeout([this, seq, offset](auto yield) {
                return backend_->fetchLedgerDiff(seq - offset, yield);
            });
        };

        rg::for_each(vs::iota(0u, numDiffs_), [&](auto i) { append(diffs, fetchDiff(i)); });
        rg::sort(diffs, [](auto const& a, auto const& b) {
            return a.key < b.key or (a.key == b.key and std::size(a.blob) < std::size(b.blob));
        });

        diffs.erase(
            std::unique(
                std::begin(diffs), std::end(diffs), [](auto const& a, auto const& b) { return a.key == b.key; }
            ),
            std::end(diffs)
        );

        std::vector<ripple::uint256> cursors{data::firstKey};
        rg::copy(
            diffs                                                                   //
                | vs::filter([](auto const& obj) { return not obj.blob.empty(); })  //
                | vs::transform([](auto const& obj) { return obj.key; }),
            std::back_inserter(cursors)
        );
        cursors.push_back(data::lastKey);  // last pair should cover the remaining range

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
