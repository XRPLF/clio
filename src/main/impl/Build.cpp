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

#include <ripple/beast/core/SemanticVersion.h>
#include <boost/preprocessor/stringize.hpp>
#include <algorithm>
#include <main/Build.h>
#include <optional>
#include <stdexcept>

namespace Build {

//--------------------------------------------------------------------------
//  The build version number. You must edit this for each release
//  and follow the format described at http://semver.org/
//------------------------------------------------------------------------------
// clang-format off
char const* const versionString = "1.0.3"
    // clang-format on
    "+"
#ifdef CLIO_BUILD
    CLIO_BUILD
#endif
#ifdef DEBUG
    ".DEBUG"
#ifdef SANITIZER
    "."
#endif
#endif

#ifdef SANITIZER
    BOOST_PP_STRINGIZE(SANITIZER)
#endif

#ifdef PKG
        "-release"
#endif
    ;

std::string const&
getClioVersionString()
{
    static std::string const value = [] {
        std::string const s = versionString;
        beast::SemanticVersion v;
        if (!v.parse(s) || v.print() != s)
            throw std::runtime_error(s + ": Bad server version string");
        return s;
    }();
    return value;
}

std::string const&
getClioFullVersionString()
{
    static std::string const value = "clio-" + getClioVersionString();
    return value;
}

}  // namespace Build
