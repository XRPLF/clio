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

#include "etl/ETLHelpers.hpp"

#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <vector>

namespace etl {
std::vector<ripple::uint256>
getMarkers(size_t numMarkers)
{
    ASSERT(numMarkers <= 256, "Number of markers must be <= 256. Got: {}", numMarkers);

    unsigned char const incr = 256 / numMarkers;

    std::vector<ripple::uint256> markers;
    markers.reserve(numMarkers);
    ripple::uint256 base{0};
    for (size_t i = 0; i < numMarkers; ++i) {
        markers.push_back(base);
        base.data()[0] += incr;
    }
    return markers;
}
}  // namespace etl
