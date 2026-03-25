#include "util/Shasum.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/digest.h>

#include <cstring>
#include <string>
#include <string_view>

namespace util {

ripple::uint256
sha256sum(std::string_view s)
{
    ripple::sha256_hasher hasher;
    hasher(s.data(), s.size());
    auto const hashData = static_cast<ripple::sha256_hasher::result_type>(hasher);
    ripple::uint256 sha256;
    std::memcpy(sha256.data(), hashData.data(), hashData.size());
    return sha256;
}

std::string
sha256sumString(std::string_view s)
{
    return ripple::to_string(sha256sum(s));
}

void
Sha256sum::update(void const* data, size_t size)
{
    hasher_(data, size);
}

ripple::uint256
Sha256sum::finalize() &&
{
    auto const hashData = static_cast<ripple::sha256_hasher::result_type>(hasher_);
    ripple::uint256 result;
    std::memcpy(result.data(), hashData.data(), hashData.size());
    hasher_ = ripple::sha256_hasher{};
    return result;
}

}  // namespace util
