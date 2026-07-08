#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <string_view>

namespace util {

[[nodiscard]] xrpl::STObject
createMPTokenNode(
    xrpl::SField const& nodeType,
    xrpl::uint192 const& issuanceID,
    std::string_view holder
);

[[nodiscard]] xrpl::STObject
createMPTokenIssuanceNode(xrpl::SField const& nodeType, std::uint32_t seq, std::string_view issuer);

}  // namespace util
