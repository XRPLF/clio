#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>

#include <string>

std::string
hexStringToBinaryString(std::string const& hex);

ripple::uint256
binaryStringToUint256(std::string const& bin);

std::string
ledgerHeaderToBinaryString(ripple::LedgerHeader const& info);
