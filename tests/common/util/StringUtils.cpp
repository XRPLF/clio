#include "util/StringUtils.hpp"

#include "rpc/RPCHelpers.hpp"

#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <string>

std::string
hexStringToBinaryString(std::string const& hex)
{
    auto const blob = xrpl::strUnHex(hex);
    std::string strBlob;

    for (auto c : *blob)  // NOLINT(bugprone-unchecked-optional-access)
        strBlob += c;

    return strBlob;
}

xrpl::uint256
binaryStringToUint256(std::string const& bin)
{
    return xrpl::uint256::fromVoid(static_cast<void const*>(bin.data()));
}

std::string
ledgerHeaderToBinaryString(xrpl::LedgerHeader const& info)
{
    auto const blob = rpc::ledgerHeaderToBlob(info, true);
    std::string strBlob;
    for (auto c : blob)
        strBlob += c;

    return strBlob;
};
