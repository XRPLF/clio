//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
// this is used by fully compatible compilers like gcc
#include <source_location>

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
// this is used by clang on linux where source_location is still not out of
// experimental headers
#include <experimental/source_location>

#else

#include <cstddef>
#include <string_view>
#endif

namespace util {

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
using SourceLocationType = std::source_location;

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
using SourceLocationType = std::experimental::source_location;

#else
// A workaround for AppleClang that is lacking source_location atm.
// TODO: remove this workaround when all compilers catch up to c++20
class SourceLocation {
    char const* file_;
    std::size_t line_;

public:
    constexpr SourceLocation(char const* file, std::size_t line) : file_{file}, line_{line}
    {
    }

    constexpr std::string_view
    file_name() const
    {
        return file_;
    }

    constexpr std::size_t
    line() const
    {
        return line_;
    }
};
using SourceLocationType = SourceLocation;
#define SOURCE_LOCATION_OLD_API

#endif

}  // namespace util

#if defined(SOURCE_LOCATION_OLD_API)
#define CURRENT_SRC_LOCATION util::SourceLocationType(__FILE__, __LINE__)
#else
#define CURRENT_SRC_LOCATION util::SourceLocationType::current()
#endif
