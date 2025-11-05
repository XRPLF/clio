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

#include "data/impl/OutputFile.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <ios>
#include <string>
#include <utility>

namespace data::impl {

OutputFile::OutputFile(std::string const& path) : file_(path, std::ios::binary | std::ios::out)
{
}

bool
OutputFile::isOpen() const
{
    return file_.is_open();
}

void
OutputFile::writeRaw(char const* data, size_t size)
{
    writeToFile(data, size);
}

void
OutputFile::writeToFile(char const* data, size_t size)
{
    file_.write(data, size);
    shasum_.update(data, size);
}

ripple::uint256
OutputFile::hash() const
{
    auto sum = shasum_;
    return std::move(sum).finalize();
}

}  // namespace data::impl
