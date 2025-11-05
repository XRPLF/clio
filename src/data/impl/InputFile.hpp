//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "util/Shasum.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <string>

namespace data::impl {

class InputFile {
    std::ifstream file_;
    util::Sha256sum shasum_;

public:
    InputFile(std::string const& path);

    bool
    isOpen() const;

    template <typename T>
    bool
    read(T& t)
    {
        return readRaw(reinterpret_cast<char*>(&t), sizeof(T));
    }

    bool
    readRaw(char* data, size_t size);

    ripple::uint256
    hash() const;
};
}  // namespace data::impl
