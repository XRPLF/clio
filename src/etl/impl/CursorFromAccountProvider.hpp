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
    CursorFromAccountProvider(
        std::shared_ptr<BackendInterface> backend,
        size_t numCursors,
        size_t pageSize
    )
        : backend_{std::move(backend)}, numCursors_{numCursors}, pageSize_{pageSize}
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
        std::vector<xrpl::uint256> cursors{data::kFirstKey};
        rg::copy(accountRoots.begin(), accountRoots.end(), std::back_inserter(cursors));
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
