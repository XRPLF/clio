#include "etl/ETLHelpers.hpp"

#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <vector>

namespace etl {
std::vector<xrpl::uint256>
getMarkers(size_t numMarkers)
{
    ASSERT(numMarkers <= 256, "Number of markers must be <= 256. Got: {}", numMarkers);

    unsigned char const incr = 256 / numMarkers;

    std::vector<xrpl::uint256> markers;
    markers.reserve(numMarkers);
    xrpl::uint256 base{0};
    for (size_t i = 0; i < numMarkers; ++i) {
        markers.push_back(base);
        base.data()[0] += incr;
    }
    return markers;
}
}  // namespace etl
