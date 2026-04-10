#pragma once

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <vector>
namespace etl::impl {

struct CursorPair {
    xrpl::uint256 start;
    xrpl::uint256 end;
};

struct BaseCursorProvider {
    [[nodiscard]] std::vector<CursorPair> virtual getCursors(uint32_t seq) const = 0;
    virtual ~BaseCursorProvider() = default;
};

}  // namespace etl::impl
