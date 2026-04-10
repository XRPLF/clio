#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/digest.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace util {
/**
 * @brief Calculates the SHA256 sum of a string.
 *
 * @param s The string to hash.
 * @return The SHA256 sum as a xrpl::uint256.
 */
xrpl::uint256
sha256sum(std::string_view s);

/**
 * @brief Calculates the SHA256 sum of a string and returns it as a hex string.
 *
 * @param s The string to hash.
 * @return The SHA256 sum as a hex string.
 */
std::string
sha256sumString(std::string_view s);

/**
 * @brief Streaming SHA-256 hasher for large data sets.
 *
 * This class provides a streaming interface for calculating SHA-256 hashes
 * without requiring all data to be in memory at once.
 */
class Sha256sum {
    xrpl::sha256_hasher hasher_;

public:
    /**
     * @brief Update hash with data.
     *
     * @param data Pointer to data to hash.
     * @param size Size of data in bytes.
     */
    void
    update(void const* data, size_t size);

    /**
     * @brief Update hash with a value.
     *
     * @param value Value to hash.
     */
    template <typename T>
    void
    update(T const& value)
    {
        update(&value, sizeof(T));
    }

    /**
     * @brief Finalize hash and return result as xrpl::uint256.
     *
     * @return The SHA-256 hash.
     */
    xrpl::uint256
    finalize() &&;
};

}  // namespace util
