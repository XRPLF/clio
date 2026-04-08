#pragma once

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
// this is used by fully compatible compilers like gcc
#include <source_location>

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
// this is used by clang on linux where source_location is still not out of
// experimental headers
#include <experimental/source_location>

#else

#include <cstddef>
#include <string_view>
#endif

namespace util {

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
using SourceLocationType = std::source_location;

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
using SourceLocationType = std::experimental::source_location;

#else
/**
 * @brief A class representing the source location of the current code
 *
 * @note This is a workaround for AppleClang that is lacking source_location atm.
 * TODO: remove this class when all compilers catch up to c++20
 */
class SourceLocation {
    char const* file_;
    std::size_t line_;

public:
    /**
     * @brief Construct a new Source Location object
     *
     * @param file The file name
     * @param line The line number
     */
    constexpr SourceLocation(char const* file, std::size_t line) : file_{file}, line_{line}
    {
    }

    /**
     * @brief Get the file name
     *
     * @return The file name
     */
    constexpr std::string_view
    file_name() const
    {
        return file_;
    }

    /**
     * @brief Get the line number
     *
     * @return The line number
     */
    constexpr std::size_t
    line() const
    {
        return line_;
    }
};
using SourceLocationType = SourceLocation;
#define SOURCE_LOCATION_OLD_API

#endif

}  // namespace util

#if defined(SOURCE_LOCATION_OLD_API)
#define CURRENT_SRC_LOCATION util::SourceLocationType(__FILE__, __LINE__)
#else
#define CURRENT_SRC_LOCATION util::SourceLocationType::current()
#endif
