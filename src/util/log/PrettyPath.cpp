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

#include "util/log/PrettyPath.hpp"

#include "util/Assert.hpp"

#include <cstddef>
#include <string_view>

namespace util {

std::string_view
prettyPath(std::string_view filePath, size_t maxDepth)
{
    ASSERT(maxDepth > 0, "maxDepth must be greater than 0");
    auto idx = filePath.size();
    while (maxDepth-- > 0) {
        idx = filePath.rfind('/', idx - 1);
        if (idx == std::string_view::npos || idx == 0)
            break;
    }
    return filePath.substr(idx == std::string_view::npos ? 0 : idx + 1);
}

}  // namespace util
