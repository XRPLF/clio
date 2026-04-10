#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>

#include <string>

std::string
hexStringToBinaryString(std::string const& hex);

xrpl::uint256
binaryStringToUint256(std::string const& bin);

std::string
ledgerHeaderToBinaryString(xrpl::LedgerHeader const& info);
