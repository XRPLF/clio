#pragma once

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "etl/impl/BaseCursorProvider.hpp"
#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>

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
    CursorFromDiffProvider(std::shared_ptr<BackendInterface> backend, size_t numCursors)
        : backend_{std::move(backend)}, numCursors_{numCursors}
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

        std::set<xrpl::uint256> liveCursors;
        std::set<xrpl::uint256> deletedCursors;
        auto i = 0;
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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

        std::vector<xrpl::uint256> cursors{data::kFirstKey};
        rg::copy(
            liveCursors | vs::take(std::min(liveCursors.size(), numCursors_)),
            std::back_inserter(cursors)
        );
        rg::sort(cursors);
        cursors.push_back(data::kLastKey);

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
