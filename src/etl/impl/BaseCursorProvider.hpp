#pragma once

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <vector>
namespace etl::impl {

struct CursorPair {
    ripple::uint256 start;
    ripple::uint256 end;
};

struct BaseCursorProvider {
    [[nodiscard]] std::vector<CursorPair> virtual getCursors(uint32_t seq) const = 0;
    virtual ~BaseCursorProvider() = default;
};

}  // namespace etl::impl
