#pragma once

#include <cstdint>

namespace util {

/**
 * @brief Convert megabytes to bytes
 * @param mb Number of megabytes to convert
 * @return The equivalent number of bytes
 */
constexpr std::uint64_t
mbToBytes(std::uint32_t mb)
{
    return mb * 1024ul * 1024ul;
}

};  // namespace util
