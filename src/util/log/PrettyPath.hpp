#pragma once

#include <cstddef>
#include <string_view>

namespace util {

/**
 * @brief Get a pretty version of the path from the source location.
 *
 * This will return the file path up to `maxDepth` parent directories.
 *
 * @param filePath The source file path
 * @param maxDepth The maximum depth of directories to include
 * @return The pretty path as a string view
 */
[[nodiscard]] std::string_view
prettyPath(std::string_view filePath, size_t maxDepth = 3);

}  // namespace util
