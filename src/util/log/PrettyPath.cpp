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
