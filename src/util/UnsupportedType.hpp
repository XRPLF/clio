#pragma once

namespace util {

/** @brief used for compile time checking of unsupported types */
template <typename>
static constexpr bool Unsupported = false;  // NOLINT(readability-identifier-naming)

}  // namespace util
