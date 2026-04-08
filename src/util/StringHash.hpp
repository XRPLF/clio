#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace util {

/**
 * @brief A string hash functor that provides transparent hash operations for various string types.
 *
 * This hash functor can be used with unordered containers to enable heterogeneous lookups
 * for different string-like types without unnecessary conversions. It supports C-style strings,
 * string views, and standard strings.
 */
struct StringHash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;  ///< Enables heterogeneous lookup

    /**
     * @brief Computes the hash of a C-style string.
     * @param str Null-terminated C-style string to hash
     * @return Size_t hash value
     */
    std::size_t
    operator()(char const* str) const;

    /**
     * @brief Computes the hash of a string_view.
     * @param str String view to hash
     * @return Size_t hash value
     */
    std::size_t
    operator()(std::string_view str) const;

    /**
     * @brief Computes the hash of a standard string.
     * @param str String to hash
     * @return Size_t hash value
     */
    std::size_t
    operator()(std::string const& str) const;
};

}  // namespace util
