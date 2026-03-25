#pragma once

#include <string>
#include <unordered_set>

namespace util {
[[nodiscard]] std::unordered_set<std::string> const&
getTxTypesInLowercase();
}  // namespace util
